# OTA, calibration & versionnement matériel — protocole

> **Statut : CONTRAT FERMÉ v1.0** — document de contrat entre le firmware (ce projet) et le
> serveur (`../sensor_server/`). Toutes les décisions D1–D14 sont closes (cf. §11) ;
> le contrat serveur détaillé vit dans `../sensor_server/docs/ota-server.md`.
> Implémentation autorisée (séquencement §10).
>
> **Related docs :** [mqtt-protocol](mqtt-protocol.md) | [architecture](architecture.md) | [configurations](configurations.md) | serveur : `../sensor_server/docs/ota-server.md`

## 0. Objectif

Permettre de flasher un nouveau firmware **par WiFi via le serveur** (OTA), tout en
garantissant que :

1. l'image reçue correspond **exactement** au matériel du device (config + révision) ;
2. **aucune donnée de calibration n'est compilée dans le firmware** — elle est stockée
   de façon rémanente sur le device et **mirrorée sur le serveur**, qui peut la
   re-pousser après une mise à jour ou une réinitialisation ;
3. le firmware devient **device-agnostique** : une même image sert toutes les unités d'un
   même type matériel.

Modèle de menace : réseau IoT **filtré, sans accès Internet**, serveur d'images interne
et de confiance → sécurité v1 = **HTTP + vérification MD5** (intégrité). Pas de TLS ni de
signature en v1 (durcissement v2 optionnel, cf. §7).

---

## 1. Identité matérielle & firmware

Trois axes **orthogonaux**. Aucun ne dépend d'un autre.

| Axe | Champ | Porté par | Change quand… |
|-----|-------|-----------|---------------|
| Type matériel (fonctionnel) | `HW_CODE` | build flag `-DHW_CODE` | le jeu de capteurs/modules change (features → binaire différent) |
| Révision matérielle (physique/élec.) | `HW_REV` | build flag `-DHW_REV` | même fonction, mais PCB/câblage/électronique qui change le binaire |
| Version firmware | `FIRMWARE_VERSION` | `[common]` semver | à chaque release |
| Identité de l'unité | `device_id` | **runtime** (voir §3) | jamais dans le firmware |
| Calibration | offsets, ratio diviseur | **runtime** (voir §4) | jamais dans le firmware |

### 1.1 `HW_CODE` — code de longueur fixe (8 car.) + registre serveur

`HW_CODE` est un code de **longueur fixe : exactement 8 caractères**, majuscules
alphanumériques (`^[A-Z0-9]{8}$`). Longueur fixe ⇒ champ serveur `CHAR(8)`, colonnes
alignées, pas de troncature à gérer. La **source de vérité** est une **base de données côté
serveur** (registre matériel, alimenté par l'API de publication, §8bis) : le serveur y garde
la description complète, la plateforme, le jeu de modules et les révisions connues. Le code
lui-même n'a donc pas besoin d'être exhaustif.

Structure `<FF><TTTTTT>` :

- `<FF>` (2 car.) — **famille plateforme** ;
- `<TTTTTT>` (6 car.) — **jeton de type**, unique dans la famille, assigné à
  l'enregistrement du matériel. Mnémonique de préférence, numérique (`000001`) en repli si
  ça devient ambigu. Peu importe : le sens est dans la DB.

| `<FF>` | Plateforme |
|--------|------------|
| `E8` | ESP8266 (D1 Mini) |
| `C3` | ESP32-C3 (Super Mini) |
| `MK` | SAMD21 (MKR WiFi 1010) |

Codes figés (D3 ✅ — seed du registre serveur via l'API A1) :

| `HW_CODE` | Ancien code | Modules (`HAS_xxx`) |
|-----------|-------------|---------------------|
| `E8BMEBAT` | `esp8266-bme280-bat` | BME280 + BATTERY + DEEP_SLEEP |
| `E8SHTBAT` | `esp8266-sht30-bat` | SHT30 + BATTERY + DEEP_SLEEP |
| `E8SHTDSP` | `esp8266-sht30-disp` | SHT30 + DISPLAY |
| `C3BMELUX` | `esp32c3-bme280-bh1750-bat` | BME280 + BH1750 + BATTERY + DEEP_SLEEP |
| `MKENVBAT` | `mkr1010-mkrenv-bat` | MKR ENV + BATTERY |

> Règle : un nouveau capteur/module ⇒ **nouveau `HW_CODE`** (nouvelle entrée du registre
> serveur, créée via l'API). Le firmware ne connaît que sa propre chaîne `HW_CODE` (8 car.).
> Le device **refuse de démarrer** si `HW_CODE` ne fait pas exactement 8 caractères (garde
> contre un build mal configuré).
>
> **Registre = strictement ce que la CI publie (D7).** Le serveur ne crée **jamais** une
> entrée de registre sur la foi d'un device. Un `HW_CODE` vu en `capabilities` mais **absent
> du registre** (firmware jamais publié par la CI) ⇒ le device est marqué **« à update »**,
> au même titre qu'un device qui ne publie aucun `HW_CODE`. Pas de stub auto-créé.

### 1.2 `HW_REV` — révision matérielle

Entier (`-DHW_REV=<n>`, défaut `1`). Absorbe les évolutions **physiques/électriques** d'un
même `HW_CODE`, y compris celles qui demandent un léger changement de code
(compilation conditionnelle `#if HW_REV >= n`). Exemples visés :

- ajout d'un **MOSFET sur le diviseur de tension batterie** (activé seulement pendant la
  mesure, pour éliminer le drain permanent) → `#if HW_REV >= 2` pilote `PIN_BATTERY_SWITCH` ;
- **remplacement du régulateur de tension** (change la référence ADC / le comportement) ;
- changement de topologie du diviseur, remap de pin, etc.

Conséquences :

- Une image est valable pour **un couple `(HW_CODE, HW_REV)` précis**. Le serveur ne
  pousse **jamais** une image d'une autre révision (risque de brick si le mapping diffère).
- Les écarts purement **numériques** issus d'une révision (nouveau ratio de diviseur après
  changement de régulateur, p.ex.) ne vont **pas** dans `HW_REV` : ce sont des **valeurs de
  calibration runtime** (§4). `HW_REV` ne gate que du **code**.

### 1.3 Version firmware

`FIRMWARE_VERSION` (semver) défini une seule fois dans `[common]` de `platformio.ini`,
bumpé à chaque release, exposé dans `capabilities.fw`.

---

## 2. Identité du device (`device_id`) — sortie du firmware

**Décision retenue : D1-b — `device_id` provisionné dans le store rémanent.**

Le `device_id` **n'est pas un build flag** : il est écrit une fois dans le store rémanent
(§3) et lu au boot. L'image reste donc **device-agnostique** (une image par
`(HW_CODE, HW_REV)`, aucun état par-unité dans le binaire), tout en **conservant les
`device_id` actuels** (`thermo_1…`) — pas de migration serveur.

Contrepartie : une **étape de provisioning** par unité (§3.1), faite une fois au premier
flash série (les cartes passent de toute façon par l'USB à ce moment-là).

> `-DDEVICE_ID` est **supprimé** des envs de production `platformio.ini`. **Exception dev (D12)** :
> si `-DDEVICE_ID` est défini (env `native`, ou build `HAS_SERIAL_DEBUG`) **et** que le store
> est vide, il est écrit une fois comme **seed** (confort dev — pas de provisioning à chaque
> flash). En production le flag est absent → le firmware refuse de publier tant qu'aucun
> `device_id` n'est provisionné (§3.1).

---

## 3. Store rémanent (calibration + éventuel `device_id`)

Zone de stockage **persistante, séparée du sketch**, qui **survit à un OTA** (l'OTA
n'écrit que la partition applicative). Le refactor s'applique à **toutes les plateformes en
une fois** (D14) — pas seulement celles qui font l'OTA ; le MKR/SAMD migre aussi ses offsets
compilés vers le store. Accès via une interface portable `IConfigStore` (impl par plateforme
+ stub mémoire natif pour les tests) :

| Plateforme | Techno (D11) | Survit à l'OTA | Survit à coupure secteur |
|------------|--------------|----------------|--------------------------|
| ESP8266 | **LittleFS** (fichier JSON, layout 4M1M) | oui | oui |
| ESP32-C3 | NVS (`Preferences`) | oui | oui |
| SAMD21 | `FlashStorage` | oui | oui |

> ⚠️ Un **effacement complet** au flash série (erase flash) vide le store → re-provisioning
> `device_id` (série) + re-push calibration (serveur). C'est le chemin de migration des
> unités existantes (§3.2).

Contenu du store (`config` runtime) :

| Clé | Type | Défaut si absent | Origine |
|-----|------|------------------|---------|
| `cal_temp` | float | `0.0` | serveur (`set_offset`) |
| `cal_humi` | float | `0.0` | serveur (`set_offset`) |
| `cal_press` | float | `0.0` | serveur (`set_offset`) |
| `bat_divider` | float | valeur nominale par `HW_REV` | serveur (`set_calibration`) |
| `device_id` | string | — (device inactif tant qu'absent) | provisioning série (§3.1) |

**Le `bat_divider` (ratio du diviseur de tension) devient une valeur de calibration
runtime** et remplace l'actuel build flag `-DBATTERY_DIVIDER_RATIO`. Valeur nominale par
défaut définie par `HW_REV` dans le firmware ; la valeur fine par unité vient du serveur.

### 3.1 Provisioning du `device_id`

Au boot, si le store ne contient pas de `device_id`, le device entre en **mode
provisioning** : il n'établit pas de session MQTT (il ne connaît pas son topic) et attend
une commande **série** :

```
provision <device_id>      # ex. provision thermo_1  → écrit le store, puis reboot
```

Fait une seule fois, à la première mise en service (accès USB déjà nécessaire pour le flash
initial). Les OTA ultérieurs n'écrivent que la partition applicative → le `device_id`
provisionné est préservé. Une commande `factory_reset` (série) efface le store pour
re-provisionner une carte réaffectée.

### 3.2 Migration des unités déjà déployées (D13)

Les unités actuelles ont `DEVICE_ID` et la calibration (`BATTERY_DIVIDER_RATIO` 2.771/2.764/
2.833, offsets MKR −2.5/−1.2/−2.6) **compilés** dans `platformio.ini`. Migration par **saisie
manuelle serveur** (parc ~5 unités, valeurs connues) :

1. **Avant reflash** : saisir dans le miroir calibration serveur, par `device_id`, les
   valeurs actuelles lues dans `platformio.ini`.
2. Flasher le firmware agnostique (série, erase) → store vide.
3. `provision <device_id>` (série) → le device se connecte, publie `cal:0`.
4. Le serveur re-pousse la calibration mirroir (§4.3) → store écrit → `cal:1`.

---

## 4. Calibration : stockage + miroir serveur + re-push

Principe : le device stocke sa calibration localement (rémanent) **et** le serveur en
garde une copie par `device_id`. Après un OTA (store normalement préservé) ou une
réinitialisation (store vide → valeurs par défaut), le serveur **re-pousse** la
calibration connue.

### 4.1 Commandes (extension de `HAS_CALIBRATION`)

| Action | Payload | Effet |
|--------|---------|-------|
| `set_offset` | `{"action":"set_offset","metric":"temp","value":-2.5}` | écrit un offset (`cal_temp/humi/press`) dans le store rémanent |
| `set_calibration` | `{"action":"set_calibration","key":"bat_divider","value":2.771}` | écrit une valeur de calibration générique (ratio diviseur…) |
| `request_calibration` | `{"action":"request_calibration"}` | le device publie sa calibration courante (voir §4.2) |

Les commandes existantes `set_offset` / `request_calibration` sont conservées ;
`set_calibration` est ajoutée pour les valeurs non-offset (ratio diviseur).

### 4.2 Rapport de calibration (device → serveur)

En réponse à `request_calibration`, le device publie sur le topic dédié
`{device_type}/{device_id}/calibration` (D2 retenu — séparé de `capabilities`) :

```json
{"cal_temp":-2.5,"cal_humi":-1.2,"cal_press":-2.6,"bat_divider":2.771}
```

Cela permet au serveur de **capturer** la calibration d'une unité déjà réglée
(bootstrap du miroir) puis de la **re-pousser** au besoin.

### 4.3 Flux de re-push après OTA / reset

```
Device (après OTA ou reset)         Serveur
──────────────────────────────────────────────────────────
store vide → cal par défaut
publish capabilities (cal:0)        ── détecte cal:0 (store vide, D8)
                                       possède le miroir calibration (par device_id)
      ◀── set_offset / set_calibration (re-push, QoS 1)
écrit store rémanent → cal:1 au prochain boot
```

> En pratique le store survit à l'OTA, donc le re-push n'est nécessaire qu'après un
> effacement (nouvelle carte, reset d'usine, changement de puce). Le miroir serveur est le
> filet de sécurité.

---

## 5. Messages `capabilities` & `commands` (découpés)

**Contrainte : `MQTT_MAX_PACKET_SIZE = 512` octets** (firmware, PubSubClient). En ajoutant
`hwrev`/`ota` et surtout la liste des commandes **avec leurs paramètres** (`command_params`),
un message unique déborde vite les 512 o. On **découpe** donc en deux messages, demandés
séparément :

### 5.1 `capabilities` — identité + métriques (court)

Réponse à `request_capabilities`. Reste petit (identité + métriques) :

```json
{
  "id":     "<serie_puce>",
  "hw":     "E8BMEBAT",
  "hwrev":  1,
  "fw":     "1.1.0",
  "ota":    1,
  "cal":    1,
  "intrvl": 300,
  "metrics": { "temp": "C", "humi": "%", "press": "hPa", "bat": "%" }
}
```

| Champ | Type | Sens |
|-------|------|------|
| `hwrev` | int | révision matérielle (§1.2) |
| `ota` | 0/1 | 1 si le device sait faire l'OTA (le serveur ne propose l'update que dans ce cas) |
| `cal` | 0/1 | **état du store de calibration (D8)** : `1` = au moins une clé de calibration écrite, `0` = aucune (carte neuve, factory reset, changement de puce). **Indépendant du `device_id`** (un device provisionné mais non calibré publie `cal:0`). Le serveur re-pousse la calibration mirroir (§4.3) uniquement sur `cal:0`. |

### 5.2 `commands` — liste des commandes + paramètres (à la demande)

Réponse à la **nouvelle commande `request_commands`**, publiée sur le topic dédié
`{device_type}/{device_id}/commands` (Device → Server) :

```json
{
  "commands": ["set_interval", "set_offset", "set_calibration", "request_calibration", "ota_update"],
  "command_params": {
    "set_interval": [{"n": "value", "t": "number"}],
    "set_offset":   [{"n": "metric", "t": "string"}, {"n": "value", "t": "number"}]
  }
}
```

Clés de paramètres **compactes `n`/`t`** (name/type) pour tenir dans les 512 o — c'est la
raison même du découpage. Seules les commandes qui **déclarent** des paramètres apparaissent
dans `command_params` ; les autres (dont `ota_update`, enregistrée sans params — son support
est déjà signalé par `ota:1` et sa présence dans `commands`) sont omises.

Si la liste dépasse encore 512 o, le device la **fragmente** (`{"part":1,"of":2,...}`) —
⟨différé : peu probable avec ≤ 8 commandes aux clés compactes⟩.

> Le serveur reconstruit ses capacités complètes en croisant les deux messages. Il envoie
> `request_capabilities` puis `request_commands` (les deux QoS 1, queuables pour un device
> endormi). Un device répond dans les 60 s à chacun.

---

## 6. Commande OTA & flux de mise à jour

### 6.1 Commande `ota_update` (serveur → device)

```json
{
  "action": "ota_update",
  "value":  "http://srv.interne/fw/E8BMEBAT/1/1.1.0.bin",
  "md5":    "<hex 32>",
  "ver":    "1.1.0",
  "hw":     "E8BMEBAT",
  "hwrev":  1
}
```

| Champ | Rôle |
|-------|------|
| `value` | URL HTTP interne de l'image |
| `md5` | intégrité — vérifiée avant bascule |
| `ver` | version cible — **garde-fou idempotence** : refus si `ver == FIRMWARE_VERSION` |
| `hw`, `hwrev` | **garde-fou compat** : refus si ≠ `HW_CODE`/`HW_REV` compilés |

Publiée en **QoS 1, `retain=false`** (queue pour device endormi via la session
persistante ; le garde-fou `ver` couvre le cas d'une commande qui traînerait).

### 6.2 Ack (device → serveur)

Le device publie sur `.../ack` **avant** de flasher :

```json
{"action":"ota_update","status":"start"}
```

En cas d'échec : `{"action":"ota_update","status":"error","message":"<motif>"}` où motif ∈
`{hw_mismatch, same_version, low_battery, download_failed, md5_mismatch}`. L'ancien
firmware reste actif (OTA atomique). Le **succès** n'est pas acké (le device reboote) : il
est constaté par le serveur au prochain `capabilities` portant la nouvelle `fw`.

### 6.3 Garde-fous device (ordre)

1. `hw`/`hwrev` ≠ compilés → `error:"hw_mismatch"`
2. `ver == FIRMWARE_VERSION` → `error:"same_version"`
3. (si `HAS_BATTERY`) `bat < OTA_MIN_BATTERY_PCT` (défaut 40) → `error:"low_battery"`
4. sinon : ack `start`, download HTTP, vérif MD5, write flash, **reboot**.

### 6.4 Flux bout-en-bout

```
Admin (serveur)              Broker                 Device (deep-sleep)
──────────────────────────────────────────────────────────────────────
choisit Firmware compatible
send_ota_update():
  valide hw/hwrev == device
  publish cmd QoS1 ────────▶ queue
                                    │ (dort ≤ publish_interval)
                                    ▼
                                  wake → publish sensors
                                  broker délivre la cmd queued
                          ◀────── ack start
                                  garde-fous OK → GET image → MD5 → REBOOT
                                  boot nouvelle fw
                          ◀────── capabilities {fw:"1.1.0"}
compare fw poussée==reçue
  → CommandLog success
  → si device réinitialisé : re-push calibration (§4.3)
```

Latence ≤ `publish_interval`. Le download bloque au-delà de `MQTT_COMMAND_WAIT_MS` (l'appel
ne rend pas la main) : aucun changement du flow deep-sleep requis.

---

## 7. Sécurité

- **v1** : HTTP + **MD5** (intégrité). Suffisant sur LAN filtré/de confiance sans Internet.
- **v2 (optionnel)** : images **signées** (RSA/SHA256 — support natif du core ESP8266/ESP32,
  clé publique compilée, refus si signature invalide). À activer si le modèle de confiance
  du LAN évolue. TLS non retenu sur ESP8266 (coût RAM BearSSL) ; la signature d'image est
  préférable.

---

## 8. Génération & publication d'une image (côté ce projet)

Une image est identifiée par **`(HW_CODE, HW_REV, FIRMWARE_VERSION)`** (device-agnostique :
`device_id` et calibration sont en store rémanent, pas dans le binaire) + son `MD5`. Le
binaire OTA est le **même `firmware.bin`** que pour le flash série
(ESP8266 et ESP32).

```bash
# 1. Bump la version : platformio.ini → [common] firmware_version = 1.1.0
# 2. Build (dans le conteneur du projet — règle Docker-only)
pio run -e <env>
# → .pio/build/<env>/firmware.bin
```

**Manifeste automatique** : un post-build PlatformIO (`extra_scripts = post:tools/gen_manifest.py`)
lit les flags de l'env et émet `firmware.json` à côté du `.bin` :

```json
{ "hw_code": "E8BMEBAT", "hw_rev": 1, "version": "1.1.0", "md5": "…", "size": 412160 }
```

**Publication** : `tools/publish_firmware.py <env>` (Python, pas bash) :

1. `pio run -e <env>` (dans le conteneur) → `firmware.bin` + `firmware.json` ;
2. calcule/relit le MD5 ;
3. **appelle l'API serveur** (§8bis) pour : upsert du `HW_CODE`/`HW_REV` dans le registre
   matériel **et** upload de l'image + métadonnées.

L'appel API est ce qui, en même temps, **actualise les bases serveur** (codes/révisions
matériels, firmwares/versions). Publier ≠ déployer : rien n'est poussé aux devices ici.

Arborescence de hosting côté serveur (device-agnostique) :

```
fw/<hw_code>/<hw_rev>/<version>.bin
```

---

## 8bis. API serveur — requis

> Requis fonctionnels de l'API HTTP interne utilisée par `publish_firmware.py`. Convergé et
> figé côté serveur — le contrat détaillé (modèles, hosting, auth) fait autorité dans
> `../sensor_server/docs/ota-server.md`. Verbes/chemins indicatifs.

**Transport & sécurité** : HTTP interne (LAN de confiance), auth par **token de publication
dédié** (en-tête `Authorization: Bearer <token>`), payloads JSON (+ binaire en multipart
pour l'upload). Ce token est **distinct de la read-API** (`api.ApiKey`, read-only) et pensé
pour un appelant **non-interactif — à terme uniquement la CI (TeamCity)** (D4). Toutes les
écritures sont **idempotentes**.

| # | Besoin | Requête indicative | Effet serveur |
|---|--------|--------------------|---------------|
| A1 | Enregistrer / mettre à jour un **code matériel** | `PUT /api/hw/codes/<hw_code>` `{platform, description, modules[]}` | upsert dans le registre matériel (DB codes) |
| A2 | Enregistrer / mettre à jour une **révision matérielle** | `PUT /api/hw/codes/<hw_code>/revs/<hw_rev>` `{description, bat_divider_nominal?, notes?}` | upsert révision (DB révisions), rattachée au code |
| A3 | **Publier une image firmware** | `POST /api/firmwares` multipart : `firmware.bin` + `{hw_code, hw_rev, version, md5, size, notes?}` | valide (code+rev existent, MD5 recalculé == fourni, taille), stocke le `.bin`, crée la ligne `Firmware`. Idempotent sur `(hw_code, hw_rev, version)` |
| A4 | **Lister** les firmwares publiés | `GET /api/firmwares?hw_code=&hw_rev=` | pour éviter les doublons / voir la dernière version |
| A5 | Récupérer la **dernière version** d'un type | `GET /api/firmwares/latest?hw_code=&hw_rev=` | aide à la décision de push (UI/CI) |

Règles :

- **A3 refuse** si `(hw_code, hw_rev)` n'existe pas encore (A1/A2 doivent précéder) → force
  la cohérence du registre.
- Le serveur **recalcule le MD5** du binaire reçu et rejette si ≠ celui annoncé (intégrité
  de bout en bout jusqu'au hosting).
- Republier la **même** `(hw_code, hw_rev, version)` : **`409` par défaut**, remplacement
  explicite via **`?overwrite=true`** (D9, tranché côté serveur).
- L'API **ne pousse rien aux devices** : le déploiement reste une action séparée (§6, `send_ota_update`).

Ce que l'API alimente côté serveur : **DB codes matériels** (A1), **DB révisions** (A2),
**DB firmwares/versions** (A3). Le `publish_firmware.py` enchaîne A1 → A2 → A3 en un appel
logique (les métadonnées viennent de `firmware.json` + `platformio.ini`).

---

## 9. Responsabilités serveur (`../sensor_server/`) — contrat

| # | Élément | Détail |
|---|---------|--------|
| 1 | Modèles registre matériel | `HardwareCode` (`hw_code` PK `CHAR(8)`, `platform`, `description`, `modules`) et `HardwareRevision` (`hw_code` FK, `hw_rev`, `description`, `bat_divider_nominal`, `notes`). Alimentés par l'API A1/A2. Valider `^[A-Z0-9]{8}$` à l'entrée. |
| 2 | Modèle `Firmware` | `hw_code`, `hw_rev`, `version`, `file`/`path`, `md5`, `size`, `uploaded_at`, `notes`. Unicité `(hw_code, hw_rev, version)`. Alimenté par A3. |
| 3 | Champs `Device` | ajouter `hw_rev` (int), `ota_capable` (bool), `calibration` (JSON miroir), `commands`/`command_params` (JSON). **Code matériel = FK seule (D10)** : `hardware_code` (**FK** → `HardwareCode`, `null` si le code claimé est absent du registre) remplace la chaîne libre `hw_code`. La claim brute **n'est pas conservée** : code inconnu → « à update » générique. `fw_version`/`hardware_id`/`display_name` existent déjà. |
| 4 | Ingestion `capabilities` | parser `hwrev`, `ota` (en plus de `id`/`hw`/`fw`) ; **ne plus** attendre `commands` ici. |
| 5 | Ingestion `commands` | nouveau topic `{type}/{id}/commands` (§5.2) → maj `Device.commands`/`command_params`. Envoyer `request_commands` (QoS 1) en plus de `request_capabilities`. |
| 6 | Ingestion `calibration` | topic `{type}/{id}/calibration` (§4.2) → maj `Device.calibration` (miroir). |
| 7 | API de publication + hosting | endpoints A1–A5 (§8bis) : registre matériel + upload/liste firmwares. **Hosting par CETTE stack** : `.bin` sous `${DATA_DIR}/firmwares/fw/…/<version>.bin` (rémanent), servi par une **`location /fw/` nginx** (pas Daphne, pas WhiteNoise) ; `OTA_BASE_URL` (env) construit le `value` de `ota_update`. |
| 8 | `send_ota_update(device, firmware)` | garde-fou `hw_code`+`hw_rev` == device ; payload §6.1 ; `_mqtt_publish(qos=1, retain=false)` ; `CommandLog(action="ota_update")`. |
| 9 | Re-push calibration | après OTA/reset détecté, renvoyer `set_offset`/`set_calibration` depuis le miroir. |
| 10 | Détection succès | `fw_version == version poussée` → `CommandLog.mark(SUCCESS)` ; timeout → `TIMEOUT`. |
| 11 | Ack OTA | `handle_ack_message` : `start` = info, `error` → `mark(FAILED, motif)`. |
| 12 | UI/admin | bouton « Pousser un firmware » : uniquement devices `ota_capable`, uniquement `Firmware` compatibles `(hw_code, hw_rev)`. |
| 13 | Migrations | `HardwareCode`, `HardwareRevision`, `Firmware` + nouveaux champs `Device`. Pas de remap `device_id` (D1-b conserve les ids). |

---

## 10. Séquencement

0. **Figer ce document** (contrat).
1. **Parallèle** : firmware portable (store calibration, `HW_REV`, module OTA, tests natifs)
   ⟂ couche données serveur (modèle `Firmware`, champs `Device`, ingestion `hwrev`/`ota`/calibration).
2. OTA plateforme (ESP8266 + ESP32-C3) ⟂ hosting + `send_ota_update` serveur.
3. Test bout-en-bout (1 ESP8266) : push → download → MD5 → reboot → capabilities → succès → re-push calibration.
4. (v2 optionnel) images signées.

---

## 11. Décisions

| ID | Décision | Statut |
|----|----------|--------|
| **D1** | Source du `device_id` (§2) | ✅ **D1-b** : provisionné en flash (§3.1), `device_id` actuels conservés, pas de migration serveur. |
| **D2** | Topic du rapport de calibration (§4.2) | ✅ topic dédié `{type}/{id}/calibration`. |
| **D3** | Codes `HW_CODE` 8 car. du registre (§1.1) | ✅ figés : `E8BMEBAT`, `E8SHTBAT`, `E8SHTDSP`, `C3BMELUX`, `MKENVBAT`. Seed du registre serveur via A1. |
| **D4** | Publication des images + auth | ✅ via **API serveur** (§8bis), appelée par `publish_firmware.py`. Auth par **token de publication dédié**, distinct de la read-API, cible **CI-only (TeamCity)**. |
| **D5** | `HW_REV` de départ | ✅ `1` partout. |
| **D6** | Découpage capabilities/commands (§5) | ✅ deux messages + commande `request_commands` (contrainte 512 o). |
| **D7** | Alimentation du registre matériel (§1.1) | ✅ **strictement la CI**. Pas de stub auto-créé ; `HW_CODE` inconnu vu en MQTT ⇒ device « à update ». |
| **D8** | Détection re-push calibration (§4.3, §5.1) | ✅ flag **`cal` (0/1)** dans `capabilities` ; re-push sur `cal:0`. Implémenté côté device, indépendant du `device_id`. |
| **D9** | Republication même version (§8bis A3) | ✅ `409` par défaut, `?overwrite=true` explicite. |
| **D10** | Champ code matériel du `Device` (§9-3) | ✅ **FK seule** `hardware_code` (null si code claimé absent du registre) ; claim brute non conservée, code inconnu → « à update » générique. `hw_rev` en int simple (pas de FK composite). |
| **D11** | Backend store ESP8266 (§3) | ✅ **LittleFS** (fichier JSON, layout 4M1M). NVS sur ESP32-C3, FlashStorage sur SAMD. Interface portable `IConfigStore`. |
| **D12** | Seed `device_id` en dev (§2) | ✅ `-DDEVICE_ID` autorisé comme **seed dev only** (env `native` / `HAS_SERIAL_DEBUG`) si store vide ; absent en production. |
| **D13** | Migration unités déployées (§3.2) | ✅ **saisie manuelle** des valeurs dans le miroir serveur avant reflash, re-push après provisioning. |
| **D14** | Périmètre refactor store (§3) | ✅ **toutes plateformes en une fois** (ESP + SAMD) ; l'OTA reste ESP-only. |
| — | Défauts adoptés | `OTA_MIN_BATTERY_PCT=40` ; partition C3 `min_spiffs.csv` (à confirmer au build) ; nominal `bat_divider` = valeur actuelle du type comme défaut `HW_REV=1` ; clés `set_calibration` = `bat_divider` (extensible) ; fragmentation `commands` différée ; fusion des envs par-unité (`sensor_8266_bmp80_1/_2` → un seul env). |
