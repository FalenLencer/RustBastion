/*
 * RUST BASTION
 * Copyright (c) 2026 Clément Gaschet. All rights reserved.
 * Unauthorized copying or distribution is prohibited.
 */

/*  net_upnp.c ─ Ouverture automatique de port (NAT-PMP + UPnP-IGD).
 *  Module ISOLÉ : tout est contenu ici (sockets bruts, HTTP, SOAP, SSDP).
 *  Voir net_upnp.h pour la procédure de retrait.
 *
 *  Cible : x86 little-endian (Windows réel + WSL/Linux pour les tests).
 *  Quelques manipulations d'adresses supposent l'octet réseau en place
 *  via memcpy (sans dépendre de l'endianness machine) — voir commentaires.
 */
#include "net_upnp.h"
#include "net_transport.h"   /* net_local_ip_be / net_ip_to_str (réutilisés) */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>       /* ShellExecuteEx / GetModuleFileName (pare-feu) */
  #include <shellapi.h>
  typedef SOCKET up_sock_t;
  #define UP_INVALID  INVALID_SOCKET
  #define up_close    closesocket
  #define UP_INPROG   (WSAGetLastError() == WSAEWOULDBLOCK)
  static void up_plat_init(void) {
      static int s = 0;
      if (!s) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); s = 1; }
  }
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>          /* getaddrinfo (énumération des interfaces) */
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  typedef int up_sock_t;
  #define UP_INVALID  (-1)
  #define up_close    close
  #define UP_INPROG   (errno == EINPROGRESS)
  static void up_plat_init(void) {}
#endif

/* ════════════════════════════════════════════════════════════════
   Outils socket portables (bloquants avec timeouts bornés)
   ════════════════════════════════════════════════════════════════ */

static void up_set_nonblock(up_sock_t s, int on) {
#ifdef _WIN32
    u_long m = on ? 1u : 0u;
    ioctlsocket(s, FIONBIO, &m);
#else
    int f = fcntl(s, F_GETFL, 0);
    if (on) fcntl(s, F_SETFL, f | O_NONBLOCK);
    else    fcntl(s, F_SETFL, f & ~O_NONBLOCK);
#endif
}

static void up_set_rcv_timeout(up_sock_t s, int ms) {
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t));
#else
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#endif
}

/* Connexion TCP bloquante avec timeout (ms). 1 = connecté. */
static int up_connect(up_sock_t s, uint32_t ip_be, uint16_t port, int timeout_ms) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = ip_be;
    a.sin_port        = htons(port);

    up_set_nonblock(s, 1);
    int r = connect(s, (struct sockaddr *)&a, sizeof(a));
    if (r == 0) { up_set_nonblock(s, 0); return 1; }
    if (!UP_INPROG) return 0;

    fd_set wf, ef;
    FD_ZERO(&wf); FD_ZERO(&ef);
    FD_SET(s, &wf); FD_SET(s, &ef);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    r = select((int)s + 1, NULL, &wf, &ef, &tv);
    if (r <= 0 || FD_ISSET(s, &ef)) return 0;

    int err = 0;
    socklen_t l = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &l);
    if (err != 0) return 0;
    up_set_nonblock(s, 0);
    return 1;
}

/* Requête HTTP brute : envoie `req` (reqlen octets) à ip:port, lit la
 * réponse (jusqu'à fermeture/timeout) dans `resp`. Retourne la taille lue,
 * -1 si échec réseau. `resp` est NUL-terminé. */
static int up_http(uint32_t ip_be, uint16_t port,
                   const char *req, int reqlen, char *resp, int respcap) {
    up_sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == UP_INVALID) return -1;
    if (!up_connect(s, ip_be, port, 3000)) { up_close(s); return -1; }
    up_set_rcv_timeout(s, 3000);

    int sent = 0;
    while (sent < reqlen) {
        int n = (int)send(s, req + sent, reqlen - sent, 0);
        if (n <= 0) { up_close(s); return -1; }
        sent += n;
    }
    int total = 0;
    while (total < respcap - 1) {
        int n = (int)recv(s, resp + total, respcap - 1 - total, 0);
        if (n > 0) total += n;
        else break;   /* 0 = fermé, <0 = timeout/erreur */
    }
    resp[total] = '\0';
    up_close(s);
    return total;
}

/* ════════════════════════════════════════════════════════════════
   Petits utilitaires texte
   ════════════════════════════════════════════════════════════════ */

/* strstr insensible à la casse. */
static const char *up_stristr(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return hay;
    }
    return NULL;
}

/* Extrait le texte entre <tag> et </tag> dans [start,end). 1 = trouvé. */
static int up_xml_tag(const char *start, const char *end,
                      const char *tag, char *out, int outcap) {
    char open[64], close[66];
    snprintf(open,  sizeof(open),  "<%s>",  tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    const char *a = strstr(start, open);
    if (!a || a >= end) return 0;
    a += strlen(open);
    const char *b = strstr(a, close);
    if (!b || b > end) return 0;
    int n = (int)(b - a);
    if (n >= outcap) n = outcap - 1;
    memcpy(out, a, n);
    out[n] = '\0';
    return 1;
}

/* 1 = code HTTP 200 sur la ligne de statut. */
static int up_http_ok(const char *resp) {
    if (!resp) return 0;
    const char *eol = strstr(resp, "\r\n");
    if (!eol) eol = resp + strlen(resp);
    const char *sp = strstr(resp, " 200");
    return sp && sp < eol;
}

/* IP publique routable ? (exclut RFC1918 + loopback + CGNAT 100.64/10 + APIPA). */
static int up_is_public(uint32_t ip_be) {
    unsigned char b[4];
    memcpy(b, &ip_be, 4);                 /* b[0..3] = octets en ordre réseau */
    if (b[0] == 10) return 0;
    if (b[0] == 127) return 0;
    if (b[0] == 172 && b[1] >= 16 && b[1] <= 31) return 0;
    if (b[0] == 192 && b[1] == 168) return 0;
    if (b[0] == 169 && b[1] == 254) return 0;           /* APIPA */
    if (b[0] == 100 && b[1] >= 64 && b[1] <= 127) return 0; /* CGNAT */
    if (b[0] == 0) return 0;
    return 1;
}

/* Parse "http://host[:port]/path" → ip_be + port + path. 1 = OK.
 * (Les box renvoient une IP littérale, pas un nom DNS → pas de résolution.) */
static int up_parse_url(const char *url, uint32_t *ip_be, uint16_t *port,
                        char *path, int pathcap) {
    if (strncmp(url, "http://", 7) != 0) return 0;
    const char *h = url + 7;
    char host[64];
    int i = 0;
    while (*h && *h != ':' && *h != '/' && i < (int)sizeof(host) - 1) host[i++] = *h++;
    host[i] = '\0';
    unsigned p = 80;
    if (*h == ':') { h++; p = (unsigned)atoi(h); while (*h && *h != '/') h++; }
    if (*h == '/') snprintf(path, pathcap, "%s", h);
    else           snprintf(path, pathcap, "/");
    struct in_addr a;
    if (inet_pton(AF_INET, host, &a) != 1) return 0;
    *ip_be = a.s_addr;
    *port  = (uint16_t)p;
    return 1;
}

/* ════════════════════════════════════════════════════════════════
   État rémanent (pour le retrait de la redirection)
   ════════════════════════════════════════════════════════════════ */
static int      g_method     = 0;      /* 0 aucun, 1 NAT-PMP, 2 UPnP */
static uint16_t g_port       = 0;
static uint32_t g_natpmp_gw  = 0;
static uint32_t g_ctrl_ip    = 0;      /* UPnP : IP du service WAN */
static uint16_t g_ctrl_port  = 0;
static char     g_ctrl_path[256];
static char     g_svc_type[96];        /* urn:...:WANIPConnection:1 */

/* ════════════════════════════════════════════════════════════════
   NAT-PMP (RFC 6886)
   ════════════════════════════════════════════════════════════════ */

/* Renvoie jusqu'à `maxn` passerelles candidates (réseau order). */
static int up_gateways(uint32_t local_be, uint32_t *out, int maxn) {
    int n = 0;
#ifndef _WIN32
    /* Linux/WSL : passerelle par défaut via /proc/net/route. */
    FILE *f = fopen("/proc/net/route", "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f)) {           /* en-tête */
            while (fgets(line, sizeof(line), f) && n < maxn) {
                char iface[32]; unsigned long dest = 1, gw = 0;
                if (sscanf(line, "%31s %lx %lx", iface, &dest, &gw) == 3 && dest == 0 && gw != 0) {
                    out[n++] = (uint32_t)gw;           /* déjà en ordre réseau (LE) */
                }
            }
        }
        fclose(f);
    }
#endif
    /* Heuristique commune (box à .1 puis .254) — utile surtout sous Windows. */
    unsigned char b[4];
    memcpy(b, &local_be, 4);
    unsigned char last[2] = { 1, 254 };
    for (int k = 0; k < 2 && n < maxn; k++) {
        unsigned char c[4]; memcpy(c, b, 4); c[3] = last[k];
        uint32_t cand; memcpy(&cand, c, 4);
        int dup = 0;
        for (int j = 0; j < n; j++) if (out[j] == cand) dup = 1;
        if (!dup) out[n++] = cand;
    }
    return n;
}

/* Échange un datagramme NAT-PMP avec gw:5351. Retourne octets reçus, -1 sinon. */
static int up_natpmp_xfer(uint32_t gw_be, const unsigned char *req, int reqlen,
                          unsigned char *resp, int respcap) {
    up_sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == UP_INVALID) return -1;
    up_set_rcv_timeout(s, 350);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = gw_be;
    a.sin_port        = htons(5351);
    int got = -1;
    for (int tries = 0; tries < 2 && got < 0; tries++) {
        if (sendto(s, (const char *)req, reqlen, 0, (struct sockaddr *)&a, sizeof(a)) < 0)
            break;
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int n = (int)recvfrom(s, (char *)resp, respcap, 0, (struct sockaddr *)&from, &fl);
        if (n > 0 && from.sin_addr.s_addr == gw_be) got = n;
    }
    up_close(s);
    return got;
}

static void up_put16(unsigned char *p, uint16_t v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void up_put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}
static uint16_t up_get16(const unsigned char *p) { return (uint16_t)((p[0] << 8) | p[1]); }

/* Tente NAT-PMP sur une passerelle : map TCP port + IP externe. 1 = succès. */
static int up_try_natpmp(uint32_t gw_be, uint16_t port, UpnpResult *out) {
    unsigned char resp[32];

    /* 1) Adresse externe : op=0. Réponse op=128, result, epoch, IP(4). */
    unsigned char areq[2] = { 0, 0 };
    int n = up_natpmp_xfer(gw_be, areq, 2, resp, sizeof(resp));
    if (n < 12 || resp[0] != 0 || resp[1] != 128 || up_get16(resp + 2) != 0)
        return 0;
    uint32_t ext_be;
    memcpy(&ext_be, resp + 8, 4);   /* déjà en ordre réseau */

    /* 2) Map TCP : op=2, internal=external=port, lifetime=7200 s. */
    unsigned char mreq[12];
    memset(mreq, 0, sizeof(mreq));
    mreq[0] = 0; mreq[1] = 2;        /* version 0, op map TCP */
    up_put16(mreq + 4, port);        /* port interne   */
    up_put16(mreq + 6, port);        /* port externe souhaité */
    up_put32(mreq + 8, 7200);        /* durée de bail  */
    n = up_natpmp_xfer(gw_be, mreq, 12, resp, sizeof(resp));
    if (n < 16 || resp[0] != 0 || resp[1] != 130 || up_get16(resp + 2) != 0)
        return 0;

    if (out) {
        out->mapped          = 1;
        out->external_ip_be  = ext_be;
        out->external_public = up_is_public(ext_be);
        snprintf(out->method, sizeof(out->method), "NAT-PMP");
    }
    g_method    = 1;
    g_port      = port;
    g_natpmp_gw = gw_be;
    return 1;
}

/* ════════════════════════════════════════════════════════════════
   UPnP-IGD (SSDP + SOAP)
   ════════════════════════════════════════════════════════════════ */

/* Horodatage monotone en millisecondes (portable). */
static unsigned up_now_ms(void) {
#ifdef _WIN32
    return (unsigned)GetTickCount();
#else
    struct timeval t; gettimeofday(&t, NULL);
    return (unsigned)(t.tv_sec * 1000u + t.tv_usec / 1000u);
#endif
}

/* Énumère les IPv4 locales (toutes interfaces) via le nom d'hôte. */
static int up_local_ips(uint32_t *out, int maxn) {
    int n = 0;
    char host[256];
    if (gethostname(host, sizeof(host)) != 0) return 0;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) return 0;
    for (struct addrinfo *p = res; p && n < maxn; p = p->ai_next) {
        if (p->ai_family != AF_INET) continue;
        uint32_t ip = ((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr;
        unsigned char b[4]; memcpy(b, &ip, 4);
        if (b[0] == 127) continue;                 /* loopback */
        int dup = 0; for (int j = 0; j < n; j++) if (out[j] == ip) dup = 1;
        if (!dup) out[n++] = ip;
    }
    freeaddrinfo(res);
    return n;
}

/* Envoie un M-SEARCH DEPUIS l'interface `local_ip` et attend une LOCATION
 * (~1,2 s). 1 = routeur IGD trouvé. Indispensable sur machine multi-cartes
 * (WSL/Hyper-V/VirtualBox) : Windows enverrait sinon le multicast sur la
 * mauvaise interface et ne joindrait jamais la vraie box. */
static int up_ssdp_one_iface(uint32_t local_ip, char *location, int cap) {
    up_sock_t s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == UP_INVALID) return 0;
    struct sockaddr_in la;
    memset(&la, 0, sizeof(la));
    la.sin_family      = AF_INET;
    la.sin_addr.s_addr = local_ip;     /* bind sur l'interface choisie */
    la.sin_port        = 0;
    bind(s, (struct sockaddr *)&la, sizeof(la));
    struct in_addr mif; mif.s_addr = local_ip;
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&mif, sizeof(mif));
    up_set_rcv_timeout(s, 400);

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &dst.sin_addr);

    static const char *targets[] = {
        "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:1",
    };
    for (int i = 0; i < 3; i++) {
        char msg[256];
        int len = snprintf(msg, sizeof(msg),
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 1\r\n"
            "ST: %s\r\n\r\n", targets[i]);
        sendto(s, msg, len, 0, (struct sockaddr *)&dst, sizeof(dst));
    }

    char fallback[256]; fallback[0] = '\0';
    int found = 0;
    unsigned t0 = up_now_ms();
    while ((up_now_ms() - t0) < 1200 && !found) {
        char buf[2048];
        int n = (int)recv(s, buf, sizeof(buf) - 1, 0);
        if (n <= 0) continue;
        buf[n] = '\0';
        const char *loc = up_stristr(buf, "LOCATION:");
        if (!loc) continue;
        loc += 9;
        while (*loc == ' ') loc++;
        char url[256]; int i = 0;
        while (*loc && *loc != '\r' && *loc != '\n' && i < (int)sizeof(url) - 1) url[i++] = *loc++;
        url[i] = '\0';
        if (up_stristr(buf, "InternetGatewayDevice") || up_stristr(buf, "WANIPConnection")
            || up_stristr(buf, "WANPPPConnection")) {
            snprintf(location, cap, "%s", url);
            found = 1;
        } else if (!fallback[0]) {
            snprintf(fallback, sizeof(fallback), "%s", url);
        }
    }
    up_close(s);
    if (!found && fallback[0]) { snprintf(location, cap, "%s", fallback); found = 1; }
    return found;
}

/* Découverte SSDP sur TOUTES les interfaces (défaut d'abord). 1 = trouvé. */
static int up_ssdp_discover(char *location, int cap) {
    uint32_t ifs[8];
    int n = 0;
    ifs[n++] = net_local_ip_be();                  /* interface route par défaut */
    uint32_t more[8];
    int m = up_local_ips(more, 8);
    for (int i = 0; i < m && n < 8; i++) {
        int dup = 0; for (int j = 0; j < n; j++) if (ifs[j] == more[i]) dup = 1;
        if (!dup) ifs[n++] = more[i];
    }
    for (int i = 0; i < n; i++)
        if (up_ssdp_one_iface(ifs[i], location, cap)) return 1;
    return 0;
}

/* Dans la description XML, trouve le service WANIP/PPPConnection :
 * remplit svc_type + control URL (chemin) + ip/port du contrôle. 1 = OK. */
static int up_find_service(const char *xml, uint32_t base_ip, uint16_t base_port,
                           uint32_t *ctl_ip, uint16_t *ctl_port, char *ctl_path, int pathcap,
                           char *svc_type, int svccap) {
    /* URLBase éventuelle (sinon base = hôte de LOCATION). */
    const char *xend = xml + strlen(xml);
    char urlbase[128] = {0};
    up_xml_tag(xml, xend, "URLBase", urlbase, sizeof(urlbase));

    const char *p = xml;
    while ((p = strstr(p, "<service>")) != NULL) {
        const char *e = strstr(p, "</service>");
        if (!e) break;
        char st[96] = {0}, cu[160] = {0};
        up_xml_tag(p, e, "serviceType", st, sizeof(st));
        up_xml_tag(p, e, "controlURL",  cu, sizeof(cu));
        if ((up_stristr(st, "WANIPConnection") || up_stristr(st, "WANPPPConnection")) && cu[0]) {
            snprintf(svc_type, svccap, "%s", st);
            if (strncmp(cu, "http://", 7) == 0) {
                return up_parse_url(cu, ctl_ip, ctl_port, ctl_path, pathcap);
            }
            /* URL relative : base = URLBase si présente, sinon LOCATION. */
            if (urlbase[0] && strncmp(urlbase, "http://", 7) == 0) {
                char dummy[8];
                up_parse_url(urlbase, ctl_ip, ctl_port, dummy, sizeof(dummy));
            } else {
                *ctl_ip   = base_ip;
                *ctl_port = base_port;
            }
            if (cu[0] == '/') snprintf(ctl_path, pathcap, "%s", cu);
            else              snprintf(ctl_path, pathcap, "/%s", cu);
            return 1;
        }
        p = e + 1;
    }
    return 0;
}

/* Envoie une action SOAP au service de contrôle. Réponse dans `resp`.
 * Retourne la taille de réponse, -1 si réseau KO. */
static int up_soap(const char *action, const char *body_inner, char *resp, int respcap) {
    char body[1024];
    snprintf(body, sizeof(body),
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
        action, g_svc_type, body_inner, action);
    int blen = (int)strlen(body);   /* longueur RÉELLE (Content-Length) */

    char host[24];
    net_ip_to_str(g_ctrl_ip, host);
    char req[2048];
    snprintf(req, sizeof(req),
        "POST %s HTTP/1.1\r\n"
        "HOST: %s:%u\r\n"
        "CONTENT-LENGTH: %d\r\n"
        "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
        "SOAPACTION: \"%s#%s\"\r\n"
        "CONNECTION: close\r\n\r\n%s",
        g_ctrl_path, host, (unsigned)g_ctrl_port, blen, g_svc_type, action, body);

    return up_http(g_ctrl_ip, g_ctrl_port, req, (int)strlen(req), resp, respcap);
}

/* Renseigne out->message (diagnostic précis) puis retourne 0. */
static int up_fail(UpnpResult *out, const char *msg) {
    if (out) snprintf(out->message, sizeof(out->message), "%s", msg);
    return 0;
}

/* Tente la voie UPnP complète. 1 = redirection établie. */
static int up_try_upnp(uint16_t port, uint32_t local_be, UpnpResult *out) {
    char location[256];
    if (!up_ssdp_discover(location, sizeof(location)))
        return up_fail(out, "Aucun routeur UPnP detecte. Activez l'UPnP dans la box, "
                            "ou autorisez le jeu dans le pare-feu Windows.");

    uint32_t base_ip; uint16_t base_port; char desc_path[256];
    if (!up_parse_url(location, &base_ip, &base_port, desc_path, sizeof(desc_path)))
        return up_fail(out, "Routeur UPnP detecte mais adresse illisible.");

    /* Description du périphérique. */
    char host[24]; net_ip_to_str(base_ip, host);
    char req[512];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHOST: %s:%u\r\nCONNECTION: close\r\n\r\n",
        desc_path, host, (unsigned)base_port);
    static char xml[16384];
    if (up_http(base_ip, base_port, req, (int)strlen(req), xml, sizeof(xml)) <= 0)
        return up_fail(out, "Routeur UPnP injoignable (pare-feu ?).");

    if (!up_find_service(xml, base_ip, base_port,
                         &g_ctrl_ip, &g_ctrl_port, g_ctrl_path, sizeof(g_ctrl_path),
                         g_svc_type, sizeof(g_svc_type)))
        return up_fail(out, "Routeur sans service WAN UPnP (mode modem/pont ?).");

    /* AddPortMapping TCP. */
    char localip[24];
    net_ip_to_str(local_be, localip);
    char inner[512];
    snprintf(inner, sizeof(inner),
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%u</NewExternalPort>"
        "<NewProtocol>TCP</NewProtocol>"
        "<NewInternalPort>%u</NewInternalPort>"
        "<NewInternalClient>%s</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>RustBastion</NewPortMappingDescription>"
        "<NewLeaseDuration>0</NewLeaseDuration>",
        (unsigned)port, (unsigned)port, localip);
    static char resp[8192];
    if (up_soap("AddPortMapping", inner, resp, sizeof(resp)) <= 0)
        return up_fail(out, "Routeur UPnP sans reponse a la demande d'ouverture.");
    if (!up_http_ok(resp))             /* 500 = SOAP fault → refus */
        return up_fail(out, "Le routeur a REFUSE l'ouverture (UPnP restreint dans la box).");

    /* IP externe (best effort : on garde même si ça échoue). */
    uint32_t ext_be = 0; int ext_pub = 0;
    if (up_soap("GetExternalIPAddress", "", resp, sizeof(resp)) > 0 && up_http_ok(resp)) {
        char ipstr[24];
        if (up_xml_tag(resp, resp + strlen(resp), "NewExternalIPAddress", ipstr, sizeof(ipstr))) {
            struct in_addr a;
            if (inet_pton(AF_INET, ipstr, &a) == 1) { ext_be = a.s_addr; ext_pub = up_is_public(ext_be); }
        }
    }
    if (out) {
        out->mapped          = 1;
        out->external_ip_be  = ext_be;
        out->external_public = ext_pub;
        snprintf(out->method, sizeof(out->method), "UPnP");
    }
    g_method = 2;
    g_port   = port;
    return 1;
}

/* ════════════════════════════════════════════════════════════════
   API publique
   ════════════════════════════════════════════════════════════════ */

int net_upnp_map_tcp(uint16_t port, UpnpResult *out) {
    up_plat_init();
    UpnpResult local;
    if (!out) out = &local;
    memset(out, 0, sizeof(*out));
    g_method = 0;

    uint32_t local_be = net_local_ip_be();

    /* 1) NAT-PMP (rapide) sur les passerelles candidates. */
    uint32_t gws[4];
    int ng = up_gateways(local_be, gws, 4);
    for (int i = 0; i < ng; i++) {
        if (up_try_natpmp(gws[i], port, out)) {
            if (out->external_public)
                snprintf(out->message, sizeof(out->message),
                         "Port ouvert via NAT-PMP. Acces distant pret.");
            else
                snprintf(out->message, sizeof(out->message),
                         "NAT-PMP OK mais IP non publique (CGNAT ?). Relais requis.");
            return 1;
        }
    }

    /* 2) UPnP-IGD (plus large). */
    if (up_try_upnp(port, local_be, out)) {
        if (out->external_ip_be == 0)
            snprintf(out->message, sizeof(out->message),
                     "Port ouvert via UPnP (IP publique inconnue).");
        else if (out->external_public)
            snprintf(out->message, sizeof(out->message),
                     "Port ouvert via UPnP. Acces distant pret.");
        else
            snprintf(out->message, sizeof(out->message),
                     "UPnP OK mais IP non publique (CGNAT ?). Relais requis.");
        return 1;
    }

    /* up_try_upnp a déjà posé un diagnostic précis ; repli générique sinon. */
    if (!out->message[0])
        snprintf(out->message, sizeof(out->message),
                 "Ouverture auto impossible (UPnP/NAT-PMP off ou CGNAT). "
                 "Essayez Tailscale ou le port-forward manuel.");
    return 0;
}

void net_upnp_unmap_tcp(uint16_t port) {
    if (g_method == 0 || g_port != port) return;
    up_plat_init();

    if (g_method == 1) {                       /* NAT-PMP : bail 0 = suppression */
        unsigned char mreq[12];
        memset(mreq, 0, sizeof(mreq));
        mreq[1] = 2;                            /* op map TCP */
        up_put16(mreq + 4, port);              /* port interne */
        up_put16(mreq + 6, 0);                 /* externe 0 + bail 0 = delete */
        up_put32(mreq + 8, 0);
        unsigned char resp[32];
        up_natpmp_xfer(g_natpmp_gw, mreq, 12, resp, sizeof(resp));
    } else if (g_method == 2) {                /* UPnP : DeletePortMapping */
        char inner[256];
        snprintf(inner, sizeof(inner),
            "<NewRemoteHost></NewRemoteHost>"
            "<NewExternalPort>%u</NewExternalPort>"
            "<NewProtocol>TCP</NewProtocol>",
            (unsigned)port);
        static char resp[4096];
        up_soap("DeletePortMapping", inner, resp, sizeof(resp));
    }
    g_method = 0;
}

/* ════════════════════════════════════════════════════════════════
   Autorisation pare-feu (Windows : règle entrante ajoutée via UAC)
   ════════════════════════════════════════════════════════════════ */
#ifdef _WIN32
/* Lance `params` via cmd.exe ÉLEVÉ (verbe "runas" → invite UAC). 1 = lancé. */
static int up_run_elevated(const char *params) {
    SHELLEXECUTEINFOA sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb       = "runas";          /* élévation → boîte de dialogue UAC */
    sei.lpFile       = "cmd.exe";
    sei.lpParameters = params;
    sei.nShow        = SW_HIDE;
    if (!ShellExecuteExA(&sei)) return 0; /* refus UAC ou erreur */
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 20000);  /* attend la fin (témoin écrit) */
        CloseHandle(sei.hProcess);
    }
    return 1;
}
#endif

int net_upnp_request_firewall(uint16_t port) {
#ifdef _WIN32
    char exe[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe)) == 0) return 0;

    /* Dossier temporaire pour le script + le témoin de résultat. */
    char tmp[MAX_PATH];
    DWORD tl = GetTempPathA((DWORD)sizeof(tmp), tmp);
    if (tl == 0 || tl >= sizeof(tmp)) return 0;

    char bat[MAX_PATH], flag[MAX_PATH];
    snprintf(bat,  sizeof(bat),  "%srb_firewall.bat", tmp);
    snprintf(flag, sizeof(flag), "%srb_firewall.ok",  tmp);
    DeleteFileA(flag);

    /* On écrit un VRAI .bat : le batch gère les quotes/chemins nativement
       (plus de pièges de parsing cmd /c). Il remplace une règle homonyme,
       autorise l'exe en entrée (couvre le port jeu + les réponses UPnP),
       écrit OK/FAIL selon le succès, puis ajoute aussi une règle de port. */
    FILE *f = fopen(bat, "wb");
    if (!f) return 0;
    fprintf(f,
        "@echo off\r\n"
        "netsh advfirewall firewall delete rule name=\"Rust Bastion\" >nul 2>&1\r\n"
        "netsh advfirewall firewall add rule name=\"Rust Bastion\" dir=in "
        "action=allow program=\"%s\" enable=yes profile=any\r\n"
        "if errorlevel 1 (echo FAIL>\"%s\") else (echo OK>\"%s\")\r\n"
        "netsh advfirewall firewall add rule name=\"Rust Bastion\" dir=in "
        "action=allow protocol=TCP localport=%u enable=yes profile=any\r\n",
        exe, flag, flag, (unsigned)port);
    fclose(f);

    char params[MAX_PATH + 16];
    snprintf(params, sizeof(params), "/c \"%s\"", bat);
    int launched = up_run_elevated(params);   /* invite UAC (cmd.exe élevé) */

    /* Vérifie le témoin : la règle a-t-elle vraiment été créée ? */
    int ok = 0;
    if (launched) {
        FILE *r = fopen(flag, "rb");
        if (r) {
            char line[16] = {0};
            if (fgets(line, sizeof(line), r)) ok = (strncmp(line, "OK", 2) == 0);
            fclose(r);
        }
    }
    DeleteFileA(bat);
    DeleteFileA(flag);
    return ok;
#else
    (void)port;
    return 1;   /* hors Windows : pare-feu géré par l'utilisateur */
#endif
}
