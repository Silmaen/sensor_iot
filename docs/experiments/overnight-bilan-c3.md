# Bilan overnight — expérience trous MQTT C3 (2026-07-22/23)

c3test = ESP32-C3 céramique + **SYNC-before-sleep** (fix en test). thermo_3 = C3 ¼λ **sans** fix (témoin). thermo_1/2 = ESP8266 (métronomes). Cadence 300 s → ~12 publishes/h attendus.


## Bilan 22:01 UTC (00:01 CEST) — fenêtre 21:01→22:01 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **40%** · thermo_3 (C3 ¼λ, sans fix) **?** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 12/12 | 0 | -60/-64.0/-57.0 | hermes:8 | — |
| thermo_2 | 8266 | loin/dehors | 12/12 | 0 | -75/-76.0/-72.0 | eudore:96 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 2/? | ? | -62/-69.0/-54.0 | hermes:6, ceryx:5 | — |
| c3test | C3 céram +SYNC | proche | 6/10 | 40 | -72/-85.0/-55.0 | hermes:5, ceryx:3 | -59 (n1) |
| thermo_mkr | MKR | loin | 11/11 | 0 | -66/-81.0/-64.0 | ceryx:94, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_2 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_3 | 8 | 13/3 | 3 | 3 | connecté mais PUBLISH non délivré (5 réveil(s)) → teardown |
| c3test | 7 | 13/1 | 7 | 70% (7/10) | — |
| thermo_mkr | 12 | 12/13 | 12 | 109% (12/11) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 23:02 UTC (01:02 CEST) — fenêtre 22:02→23:02 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **?** · thermo_3 (C3 ¼λ, sans fix) **?** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 13/13 | 0 | -58/-69.0/-54.0 | hermes:16, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 13/13 | 0 | -75/-76.0/-73.0 | eudore:96 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 2/? | ? | -62/-69.0/-55.0 | hermes:15, ceryx:6 | -55 (n1) |
| c3test | C3 céram +SYNC | proche | 2/? | ? | -74/-84.0/-63.0 | hermes:13, ceryx:4 | -57 (n1) |
| thermo_mkr | MKR | loin | 12/12 | 0 | -65/-65.0/-64.0 | ceryx:92, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 14 | 28/0 | 14 | 108% (14/13) | trajet complet OK |
| thermo_2 | 14 | 28/0 | 14 | 108% (14/13) | trajet complet OK |
| thermo_3 | 7 | 9/5 | 2 | 2 | connecté mais PUBLISH non délivré (5 réveil(s)) → teardown |
| c3test | 7 | 10/4 | 2 | 2 | connecté mais PUBLISH non délivré (5 réveil(s)) → teardown |
| thermo_mkr | 12 | 12/13 | 12 | 100% (12/12) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 00:03 UTC (02:03 CEST) — fenêtre 23:03→00:03 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **29%** · thermo_3 (C3 ¼λ, sans fix) **?** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 12/12 | 0 | -58/-59.0/-57.0 | hermes:14, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 12/12 | 0 | -76/-76.0/-73.0 | eudore:100 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 1/? | ? | -56/-56.0/-56.0 | hermes:12, ceryx:6 | — |
| c3test | C3 céram +SYNC | proche | 10/14 | 29 | -72/-86.0/-63.0 | hermes:9, ceryx:8 | -59 (n1) |
| thermo_mkr | MKR | loin | 12/12 | 0 | -65/-65.0/-64.0 | ceryx:93, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_2 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_3 | 5 | 6/4 | 1 | 1 | connecté mais PUBLISH non délivré (4 réveil(s)) → teardown |
| c3test | 8 | 13/3 | 10 | 71% (10/14) | — |
| thermo_mkr | 13 | 13/13 | 13 | 108% (13/12) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 01:02 UTC (03:02 CEST) — fenêtre 00:02→01:02 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **?** · thermo_3 (C3 ¼λ, sans fix) **?** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 12/12 | 0 | -58/-59.0/-57.0 | hermes:5, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 12/12 | 0 | -76/-76.0/-75.0 | eudore:102 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 0/? | ? | — | ceryx:6, hermes:5 | — |
| c3test | C3 céram +SYNC | proche | 2/? | ? | -74/-84.0/-63.0 | ceryx:9, hermes:5 | -59 (n1) |
| thermo_mkr | MKR | loin | 12/12 | 0 | -65/-65.0/-64.0 | ceryx:95, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_2 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_3 | 5 | 5/4 | 0 | 0 | connecté mais PUBLISH non délivré (5 réveil(s)) → teardown |
| c3test | 6 | 8/4 | 2 | 2 | connecté mais PUBLISH non délivré (4 réveil(s)) → teardown |
| thermo_mkr | 13 | 13/13 | 13 | 108% (13/12) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 02:02 UTC (04:02 CEST) — fenêtre 01:02→02:02 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **31%** · thermo_3 (C3 ¼λ, sans fix) **?** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 12/12 | 0 | -57/-58.0/-56.0 | hermes:5, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 13/13 | 0 | -75/-76.0/-73.0 | eudore:105 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 1/? | ? | -54/-54.0/-54.0 | ceryx:6, hermes:5 | — |
| c3test | C3 céram +SYNC | proche | 9/13 | 31 | -72/-84.0/-63.0 | ceryx:13, hermes:4 | — |
| thermo_mkr | MKR | loin | 11/11 | 0 | -65/-65.0/-64.0 | ceryx:94, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_2 | 14 | 28/0 | 14 | 108% (14/13) | trajet complet OK |
| thermo_3 | 6 | 7/5 | 1 | 1 | connecté mais PUBLISH non délivré (5 réveil(s)) → teardown |
| c3test | 7 | 12/2 | 9 | 69% (9/13) | — |
| thermo_mkr | 12 | 12/12 | 12 | 109% (12/11) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 03:02 UTC (05:02 CEST) — fenêtre 02:02→03:02 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **27%** · thermo_3 (C3 ¼λ, sans fix) **33%** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 13/13 | 0 | -57/-58.0/-56.0 | hermes:5, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 12/12 | 0 | -75/-76.0/-73.0 | eudore:108 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 4/6 | 33 | -53/-54.0/-53.0 | ceryx:6, hermes:4 | — |
| c3test | C3 céram +SYNC | proche | 8/11 | 27 | -76/-84.0/-63.0 | ceryx:18, hermes:4 | -57 (n1) |
| thermo_mkr | MKR | loin | 13/13 | 0 | -65/-65.0/-64.0 | ceryx:95, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 14 | 28/0 | 14 | 108% (14/13) | trajet complet OK |
| thermo_2 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_3 | 7 | 8/6 | 4 | 67% (4/6) | connecté mais PUBLISH non délivré (3 réveil(s)) → teardown |
| c3test | 12 | 20/4 | 12 | 109% (12/11) | trajet complet OK |
| thermo_mkr | 14 | 14/13 | 14 | 108% (14/13) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.


## Bilan 04:02 UTC (06:02 CEST) — fenêtre 03:02→04:02 UTC (1 h)

**Synthèse** : c3test (C3 céram +SYNC) **100%** · thermo_3 (C3 ¼λ, sans fix) **14%** · thermo_1 (8266) **0%** · thermo_2 (8266 loin) **0%** · thermo_mkr **0%**. Lecture : si **c3test ≪ thermo_3**, la synchro-avant-sleep corrige les trous.

### 1. Réseau par device (API + AP)

| device | plateforme | lieu | publishes/att. | perte % | RSSI RX moy/min/max | AP utilisés (assoc) | TX RSSI AP (now) |
|---|---|---|---|---|---|---|---|
| thermo_1 | 8266 | proche | 12/12 | 0 | -57/-57.0/-55.0 | hermes:5, ceryx:1 | — |
| thermo_2 | 8266 | loin/dehors | 12/12 | 0 | -75/-76.0/-75.0 | eudore:109 | — |
| thermo_3 | C3 ¼λ (témoin) | proche | 6/7 | 14 | -54/-54.0/-53.0 | hermes:5, ceryx:5 | — |
| c3test | C3 céram +SYNC | proche | 10/2017 | 100 | -65/-85.0/-63.0 | ceryx:20, hermes:4 | -57 (n1) |
| thermo_mkr | MKR | loin | 12/12 | 0 | -65/-65.0/-64.0 | ceryx:97, eudore:1 | — |

### 2. Trajet MQTT (broker + web, dernière heure)

| device | broker CONNECT | disconnect clean/timeout | web PUBLISH ingérés | bout-en-bout | où ça s'arrête |
|---|---|---|---|---|---|
| thermo_1 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_2 | 13 | 26/0 | 13 | 108% (13/12) | trajet complet OK |
| thermo_3 | 11 | 14/9 | 7 | 100% (7/7) | connecté mais PUBLISH non délivré (4 réveil(s)) → teardown |
| c3test | 8 | 12/4 | 11 | 1% (11/2017) | — |
| thermo_mkr | 13 | 13/13 | 13 | 108% (13/12) | trajet complet OK |

### 3. Notes

- TX RSSI AP = `signal` vu par l'AP à l'instant du bilan (deep-sleep → seuls les devices réveillés à ce moment sont vus).
- « où ça s'arrête » : CONNECT sans PUBLISH ingéré = publish perdu au teardown (hypothèse). c3test = build SYNC (confirmation avant sleep) ; thermo_3 = C3 sans fix (témoin).
- ⚠️ c3test (SYNC) **republie** jusqu'à 4× par réveil non confirmé → son « web PUBLISH ingérés » est **gonflé par les resends** ; sa vraie métrique = la **perte API** (readings distincts arrivés). Un resend qui finit par passer = un trou évité (c'est le but du fix).
- « disconnect clean/timeout » peut dépasser le nb de CONNECT (chaque reconnexion génère un `disconnected: session taken over` de l'ancienne session) — indicatif, pas exact.
