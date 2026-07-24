# Bilan nuit — thermo_3 sur firmware « fix complet » (2026-07-23 → 24)

## Contexte

thermo_3 (ESP32-C3, `C3BMELUX`, antenne céramique + fil ¼λ, batterie 2S) flashé avec
`sensor_c3_bme280_bh1750_sync` intégrant **les trois apports** de la session :

| Apport                        | Effet attendu                                                              |
|-------------------------------|----------------------------------------------------------------------------|
| Instrumentation `wf/mf/sf/bx` | Attribue chaque réveil perdu : WiFi / MQTT / capteur / boot non-deep-sleep |
| Retry MQTT (3×, socket 5 s)   | Récupère les échecs de connexion MQTT **isolés/transitoires**              |
| Wait-for-IP (connect_wifi)    | N'attaque MQTT qu'avec une IP DHCP valide (tue les `mf` « sans IP »)       |

SYNC-before-sleep actif (build flag). Intervalle **300 s** (batterie). **confirm mode ON**
(diag à chaque réveil : coût marginal car SYNC fait déjà le loopback, visibilité totale).

## Objectif

Mesurer sur une nuit, sur le **nœud terrain réel**, si le trio fait fondre la perte, et attribuer ce qui reste (`mf`/
`wf`/rafales) — avec corrélation **AP-side** (roam/signal réel).

## Référence pré-fix (à battre)

- thermo_3 sans fix : **~62 % de perte en journée**, **~81 % la nuit** (métrique nominale 300 s).
- Mécanisme : `mf` (échec connexion TCP/MQTT), 2 régimes — isolés (retry corrige) + rafales de ~min (TCP n'atteint pas
  le broker sur lien fort ; cause pile réseau, possible infra).
- 8266 co-localisés : **0 %** (métronomes).

## État initial

| Heure (local) | bat | batv  | rssi (self) | note                         |
|---------------|-----|-------|-------------|------------------------------|
| 22:08:56      | 72% | 7.72V | -47         | boot fix firmware, 1 publish |

## Bilans horaires

_(remplis au fil de la nuit ; métrique de perte = cadence nominale 300 s, PAS la médiane)_

### 23:00 — heure 1 (boot ~22:08 → 23:00, ~52 min, 11 réveils)

| Mesure              | Valeur                                             |
|---------------------|----------------------------------------------------|
| Livré               | **10/11 (~91 %)** (API 10 sensors ; device seq=10) |
| `mf` / `wf` / `sf`  | **1** / 0 / 0                                      |
| `bx` (boots non-DS) | 1 (boot cold uniquement)                           |
| Sauvetages retry    | **2** (boot3 wake 11,5 s ; boot6 wake 14,2 s)      |
| AP-side             | hermes seul, **-47 dBm**, aucun roam               |
| Batterie            | 7.72 V → 7.65 V                                    |

**Lecture :** 1 seul `mf` isolé (boot 7), pas de rafale ; 2 connexions marginales récupérées par le retry (réveils
longs → livrés). Vs baseline pré-fix (~62 % jour /
~81 % nuit de perte), **~9 % de perte = amélioration nette**. ⚠️ *Mais* : heure de bon RF (signal fort -47), et thermo_3
a pu être **repositionné** lors du reflash → à prendre comme **encourageant mais précoce** ; le vrai test = la nuit
entière, heures creuses incluses.

### 00:00 — heure 2 (23:00 → 00:00, ~12 réveils)

| Mesure             | Valeur                                                               |
|--------------------|----------------------------------------------------------------------|
| Livré              | **12/12 (~100 %)** (API)                                             |
| `mf` / `wf` / `sf` | **0 nouveau** (cumul 1 / 0 / 0)                                      |
| Sauvetages retry   | 0 (réveils ~3,1 s, connexions directes)                              |
| AP-side            | logger vide — deauth propre → poll 30 s rate le réveil (best-effort) |
| Batterie           | 7.65 V → 7.65 V (plate)                                              |
| Cumul depuis boot  | boot 22 / seq 21 / `mf` 1 → **~95 % livré**                          |

**Lecture :** 2ᵉ heure **propre à 100 %**, aucun nouvel échec, batterie stable (le SYNC + confirm ne drainent pas
visiblement). Conditions RF toujours bonnes. La limite du logger AP (deauth propre) n'affecte que la corrélation AP — de
toute façon la cause des rafales (stall TCP sur hermes-fort) est déjà établie.

### 01:00 — heure 3 (00:00 → 01:00, ~12 réveils) — *fenêtre jadis critique*

| Mesure             | Valeur                                            |
|--------------------|---------------------------------------------------|
| Livré              | **11/12 (~92 %)** (API)                           |
| `mf` / `wf` / `sf` | **+1 `mf`** (boot 25, isolé) / 0 / 0              |
| Sauvetages retry   | **2** (boot23 5,7 s ; boot24 14,3 s)              |
| AP-side            | **hermes seul, -47 dBm, aucun roam** (11 échant.) |
| Batterie           | 7.65 V → 7.65 V (plate)                           |
| Cumul depuis boot  | boot 34 / seq 32 / `mf` 2 → **~94 % livré**       |

**Lecture :** la tranche 00-01 h (où le C3 tombait à **~81 % de perte** la nuit précédente) est ici à **~92 % livré,
sans rafale**, 1 `mf` isolé récupéré au voisinage par le retry. ⚠️ *Bémol honnête* : thermo_3 reste à **-47 dBm sur
hermes** (fort) toute la nuit, alors que la nuit passée il roamait vers ceryx en signal faible. L'absence de rafale
tient donc **au firmware ET à une position favorable** (probable repositionnement au reflash) — le fix contribue
clairement (retry-saves visibles), mais on ne peut pas lui créditer seul l'absence de rafale.

### 02:00 — heure 4 (01:00 → 02:00, ~12 réveils) — *1ers signes de dégradation*

| Mesure             | Valeur                                                  |
|--------------------|---------------------------------------------------------|
| Livré              | **~9/12 (~75 % API ; ~83 % comptage `mf`)** — en baisse |
| `mf` / `wf` / `sf` | **+2 `mf`** (boot 37-38, mini-rafale) / 0 / 0           |
| Sauvetages retry   | **2** (boot36 11,2 s ; boot46 5,7 s)                    |
| AP-side            | 20× hermes -47 **+ 1× ceryx -77 dBm @01:44 (ROAM)**     |
| self-RSSI          | plus rugueux (-68/-70 fréquents)                        |
| Batterie           | 7.65 V → 7.66 V (plate)                                 |
| Cumul depuis boot  | boot 46 / seq 43 / `mf` 4 → **~91 % livré**             |

**Lecture :** le stress nocturne attendu commence — **roam vers ceryx (-77, faible)**

+ mini-rafale de 2 réveils. **Mais le fix tient ~75-83 %** là où la nuit précédente cette tranche s'effondrait à **~19 %
  de livraison (~81 % perte)**. Le retry absorbe l'essentiel du coup dur ; le roam est corrélé à la mini-rafale (lien
  ceryx faible → connect plus dur).

### 03:00 — heure 5 (02:00 → 03:00, ~12 réveils) — *cœur de la fenêtre creuse*

| Mesure             | Valeur                                      |
|--------------------|---------------------------------------------|
| Livré              | **12/12 (100 %)** (API)                     |
| `mf` / `wf` / `sf` | **0 nouveau** (cumul 4 / 0 / 0)             |
| Sauvetages retry   | 0 (connexions directes ~3,1 s)              |
| AP-side            | logger vide (deauth propre — best-effort)   |
| Batterie           | 7.65 V → 7.64 V (plate)                     |
| Cumul depuis boot  | boot 57 / seq 54 / `mf` 4 → **~93 % livré** |

**Lecture :** retour à **100 %** sur le cœur de la fenêtre creuse (02-03 h, la plus dure d'ordinaire). Le dip de l'heure
4 (roam ceryx) était **transitoire**. La nuit reste très au-dessus du baseline pré-fix.

### 04:00 — heure 6 (03:00 → 04:00, ~12 réveils) — *retry vs roam*

| Mesure             | Valeur                                             |
|--------------------|----------------------------------------------------|
| Livré              | **12/12 (100 %)** (API)                            |
| `mf` / `wf` / `sf` | **0 nouveau** (cumul 4 / 0 / 0)                    |
| Sauvetages retry   | **2** (boot59 14,5 s ; boot64 11,5 s)              |
| AP-side            | 2× hermes -47 **+ 1× ceryx -76 dBm @03:58 (ROAM)** |
| Batterie           | 7.63 V → 7.62 V (déclin lent)                      |
| Cumul depuis boot  | boot 69 / seq 66 / `mf` 4 → **~94 % livré**        |

**Lecture :** un **roam vers ceryx (-76, faible)** cette heure aussi, mais **100 % livré** — les 2 réveils longs sont le
retry qui **récupère la connexion sur le lien faible du roam**. C'est précisément le scénario qui effondrait le C3 la
nuit passée. Batterie : ~-0,10 V sur 6 h (le SYNC+confirm coûtent peu).

### 05:00 — heure 7 (04:00 → 05:00, ~10 réveils) — *résidu sur lien fort*

| Mesure             | Valeur                                         |
|--------------------|------------------------------------------------|
| Livré              | **~8/10 (~80 %)** (API 8)                      |
| `mf` / `wf` / `sf` | **+2 `mf`** (sur lien FORT) / 0 / 0            |
| Sauvetages retry   | 0 visible                                      |
| AP-side            | **41× hermes -43 dBm (très fort), aucun roam** |
| Batterie           | 7.63 V (plate)                                 |
| Cumul depuis boot  | boot 79 / seq 75 / `mf` 6 → **~92 % livré**    |

**Lecture :** dip à ~80 %, mais **cette fois pas de roam** — l'AP voit thermo_3 à **-43 dBm (très fort)**. Ces 2 `mf`
sont donc le **stall TCP résiduel sur bon lien**
(ni RF, ni roam) que le retry in-wake ne rattrape pas. Confirme les deux régimes : le fix absorbe la majorité, ce
résidu-là reste (piste infra/pile réseau, à part).

### 06:00 — heure 8 (05:00 → 06:00, ~13 réveils) — *grosse rafale, lien fort*

| Mesure             | Valeur                                                |
|--------------------|-------------------------------------------------------|
| Livré              | **~5/13 (~38 %)** (API 5)                             |
| `mf` / `wf` / `sf` | **+8 `mf`** (boot 88 : miss=8, rafale) / 0 / 0        |
| AP-side            | **69× hermes -45 dBm (fort), aucun roam**             |
| Batterie           | bat 67 %                                              |
| Compteurs finaux   | boot 92 / seq 80 / `mf` 14 / `wf` 0 / `sf` 0 / `bx` 1 |

**Lecture :** vraie rafale de **8 `mf`** — mais **encore sur lien fort** (-45, hermes, aucun roam). C'est le **stall TCP
résiduel** dominant, que ni le retry ni le wait-for-IP ne rattrapent (cause hors-device probable : pile/infra).

## Bilan final (matin)

**Nuit ~7,8 h, thermo_3 sur firmware fix (retry MQTT + wait-for-IP + instrumentation).**

| Global                       | Valeur                                                    |
|------------------------------|-----------------------------------------------------------|
| Livré                        | **~78/92 réveils ≈ 82-85 %** (API 78 ; boot 92 − `mf` 14) |
| `mf` (échecs connexion MQTT) | **14** — *tous* `mf`, **`wf` 0, `sf` 0**                  |
| Double-boot (`bx`)           | **1** (seul le boot cold) → aucun réveil parasite         |
| Batterie                     | 7.72 V → 7.62 V (**-0,10 V / 7,8 h**, modéré)             |

**Perte par heure :** 91 / 100 / 92 / 75-83 / 100 / 100 / 80 / 38 %. Deux heures (H4, H8) concentrent **10 des 14 `mf`**
et les deux rafales.

**Verdict :**

- **Gain massif vs baseline pré-fix** : ~82-85 % livré cette nuit contre **~19-38 %**
  (perte ~62-81 %) sans fix. Le trio retry+wait-for-IP **convertit la majorité des connexions marginales/transitoires en
  livraisons** (plusieurs sauvetages retry visibles = réveils longs qui aboutissent, y compris sur roam ceryx à -76).
- **Toutes les pertes sont des `mf`** (connexion TCP/MQTT) — jamais WiFi (`wf`=0), jamais capteur (`sf`=0), jamais
  double-boot. La localisation tient.
- **Résidu non résolu = rafales de `mf` sur lien FORT** (H8 : 8 `mf` à -45 dBm, hermes, sans roam). Ni RF, ni roam, ni
  antenne → **stall TCP au niveau pile/infra**, hors de portée du retry in-wake. **C'est le prochain chantier**
  (candidat roadmap :
  reset réseau plus profond sur `mf` répétés, ou piste infra DHCP/conntrack AP).

**Témoin co-localisé (même nuit, même endroit — position CONTRÔLÉE) :**

| Device (côte à côte)    | reçus/attendus | perte     | RX moy  |
|-------------------------|----------------|-----------|---------|
| thermo_1 (8266)         | 97/95          | **~0 %**  | -51 dBm |
| thermo_3 (C3 + fix)     | 79/96          | **~18 %** | -52 dBm |

thermo_3 est à la **même place que la nuit no-fix** (et depuis plusieurs jours à côté de thermo_1). Donc : (1)
l'amélioration ~62-81 % → ~18 % de perte est **due au fix** (position contrôlée) ; (2) le résidu ~18 % est bien
**plateforme C3** — à signal égal (-51/-52), le 8266 fait 0 %, le C3 fixé fait 18 %. *(Corrige un bémol antérieur qui
attribuait à tort l'amélioration à une meilleure position.)*

**Nature des `mf` :** `connect_mqtt()` (PubSubClient.connect) renvoie faux. Le journal broker (aucun `New client
connected` pendant les rafales) montre que **la connexion TCP n'atteint jamais le broker** → échec d'**établissement
TCP** (probable state `-2 CONNECT_FAILED`), **pas** un rejet MQTT auth/protocole. Non prouvé au code près : le compteur
`mf` n'enregistre pas le `state()` (logué en série seulement, illisible ici). → **à instrumenter** (compteurs par motif
ou dernier code dans le `diag`).

**Autres bémols :**

- Métrique de livraison = borne haute (les resends SYNC gonflent le compte) ; « réveils livrés » (boot − mf) ≈ 85 % est
  le plus fiable.
- Logger AP-side best-effort (deauth propre → certaines heures 0 échantillon).
- Batterie : coût `confirm`+SYNC visible mais modéré ; en prod (`confirm` off) ce serait moindre.
