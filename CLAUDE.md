# Thermo - Projet IoT Thermostat

## Aperçu

Projet PlatformIO modulaire multi-plateforme pour des capteurs IoT. Supporte ESP8266 (D1 Mini), ESP32-C3 (Super Mini)
et SAMD21 (Arduino MKR WiFi 1010). Architecture basée sur des feature flags (`HAS_xxx`) permettant de composer des
variantes hardware à partir d'un noyau MQTT commun.

## Architecture modulaire

Le projet s'articule autour de :

- **Noyau MQTT** (`src/main.cpp`) : connexion, publication, commandes, capabilities — toujours présent
- **Modules optionnels** activés par feature flags dans `platformio.ini` :

| Flag               | Module                                 | Metrics ajoutées (noms wire, courts) | Commandes ajoutées                                     |
|--------------------|----------------------------------------|--------------------------------------|--------------------------------------------------------|
| `HAS_BME280`       | Capteur BME/BMP280                     | `temp`, `humi`, `press`              | —                                                      |
| `HAS_SHT30`        | Capteur SHT30 (shield Wemos)           | `temp`, `humi`                       | —                                                      |
| `HAS_MKR_ENV`      | MKR ENV Shield (4 capteurs)            | `temp`, `humi`, `press`, `lux`, `uv` | —                                                      |
| `HAS_BATTERY`      | Monitoring batterie                    | `bat`, `batv`                        | `calibrate_battery`                                    |
| `HAS_LIGHT`        | Capteur luminosité (analog)            | `light`                              | —                                                      |
| `HAS_BH1750`       | Capteur lux BH1750 (I2C)               | `lux`                                | —                                                      |
| `HAS_CALIBRATION`  | Offsets capteurs (cross-module)        | —                                    | `set_offset`, `set_calibration`, `request_calibration` |
| `HAS_RELAY`        | Dual relay board (2 ch.)               | `relay1`, `relay2`                   | `relay_toggle`, `relay_contact`                        |
| `HAS_DISPLAY`      | Afficheur 7-segments + bouton          | —                                    | —                                                      |
| `HAS_DEEP_SLEEP`   | Deep sleep entre lectures (ESP8266/C3) | —                                    | —                                                      |
| `HAS_OTA`          | Mise à jour firmware WiFi (ESP only)   | —                                    | `ota_update`                                           |
| `HAS_SERIAL_DEBUG` | Logs debug verbose sur série           | —                                    | —                                                      |

> **Note** : `HAS_BME280`, `HAS_SHT30` et `HAS_MKR_ENV` sont mutuellement exclusifs. Les métriques device
> utilisent les noms **courts** (`temp`, `bat`…) ; le serveur les stocke sous leurs noms canoniques
> (`temperature`, `bat_percent`…).

- **Identité device** :
    - `device_id` : **provisionné au runtime** (série `provision <id>`, stocké dans le config store) — ce
      n'est plus un build flag. `-DDEVICE_ID` ne subsiste que comme *seed* sur les builds dev
      (`HAS_SERIAL_DEBUG`) et l'env `native`.
    - `-DMQTT_DEVICE_TYPE` (`platformio.ini`) : catégorie de device, sert de préfixe aux topics.
    - `-DHW_CODE` : code hardware **8 caractères** `^[A-Z0-9]{8}$`, **identique pour un même hardware**.
      Une image firmware par couple `(HW_CODE, HW_REV)`.
    - `-DHW_REV` : révision hardware (entier, `1` partout actuellement). Garde le code `#if HW_REV >= n`
      (ex. HW rev 2 des nodes 2S = régulateur HT7350 + MOSFET dans le pont diviseur).
    - `FIRMWARE_VERSION` : version semver globale (dans `[common]`), bumpée à chaque release.
    - Calibration (offsets, `bat_divider`) : runtime aussi (config store + miroir serveur), jamais compilée.
    - `id` (n° série puce), `hw`, `hwrev`, `fw`, `ota`, `cal` + les metrics sont exposés dans `capabilities`.

## Environnements PlatformIO

```bash
# ESP8266 (D1 Mini)
pio run -e sensor_8266_empty          # Build : BME280 + debug (dev)
pio run -e sensor_8266_bmp80          # Build : BME280 + batterie + deep sleep
pio run -e sensor_8266_sht30          # Build : SHT30 + batterie + deep sleep
pio run -e sensor_8266_display_sht30  # Build : SHT30 + afficheur + debug
pio run -e sensor_8266_bmp80 -t upload  # Upload ESP8266

# ESP32-C3 (Super Mini)
pio run -e sensor_c3_bme280_bh1750    # Build : BME280 + BH1750 + batterie + deep sleep
pio run -e sensor_c3_bme280_bh1750 -t upload  # Upload ESP32-C3

# SAMD21 (Arduino MKR WiFi 1010 + MKR ENV Shield)
pio run -e sensor_mkr_env             # Build : MKR ENV Shield + batterie 1S (duty-cycle)
pio run -e sensor_mkr_env -t upload   # Upload MKR

# Utilitaires
pio run -e cell_tester_mkr            # Build : testeur de cellules 18650 (MKR, série)
pio run -e cell_tester_mkr -t upload  # Upload testeur

# Tests & monitoring
pio run -e native              # Build natif (tests)
pio test -e native             # Exécuter les tests unitaires (149 tests)
pio device monitor             # Moniteur série (115200 baud)
```

**Note :** L'intervalle de publication par défaut est 10s avec `HAS_SERIAL_DEBUG` (dev), 300s (5 min)
sans (production). Configurable à distance via la commande MQTT `set_interval`.

## Structure

```bash
src/
  main.cpp              - Orchestrateur (seul fichier de "glue", #ifdef HAS_xxx)
  cell_tester.cpp       - Firmware testeur de cellules 18650 (MKR, standalone)
include/
  credentials.h         - WiFi & MQTT credentials (gitignored, + credentials.h.example)
lib/
  thermo_core/          Portable, testable, 0 dépendance Arduino
    src/
      config.h               - Pins, ADC, timing, seuils batterie (#defines purs)
      debug.h                - Macros debug série (HAS_SERIAL_DEBUG)
      interfaces/            - Interfaces abstraites (ISensor, INetwork, ISleep, IConfigStore)
      modules/               - Modules register/contribute (voir tableau ci-dessus, dont ota_module)
      module_registry.*      - Registre de modules (metrics, commands, handlers)
      payload_builder.*      - Construction JSON incrémentale
      mqtt_payload.*         - Formatage payloads (capabilities/commands) & parsing commandes
      device_topics.*        - Construction runtime des topics (device_type + device_id provisionné)
      config_store.*         - Clés + store de config runtime (device_id, offsets, bat_divider)
      battery.*              - Maths ADC→tension→SoC (portable, inclut config.h)
      sensor_data.h          - Struct données capteur
      display_encoding.*     - Encodage BCD 7-segments
  thermo_drivers/       Drivers capteurs cross-plateforme (Arduino, I2C/SPI)
    src/
      bme280_sensor.*        - BME/BMP280 I2C driver
      sht30_sensor.*         - SHT30 I2C driver
      bh1750_sensor.*        - BH1750 I2C lux sensor driver
      mkr_env_sensor.*       - MKR ENV Shield driver
      shift_display.*        - Afficheur 7-segments via shift registers
  thermo_platform/      Code spécifique à UNE plateforme
    src/
      esp8266/               - WiFi, deep sleep + RTC memory, OTA (ESPhttpUpdate), LittleFS config store
      esp32/                 - WiFi (Arduino-ESP32), deep sleep, OTA (HTTPUpdate), NVS config store
      samd/                  - WiFi (WiFiNINA), standby (RTCZero), FlashStorage config store (pas d'OTA)
      battery_adc.*          - Lecture ADC batterie unifiée (#if plateforme)
      hw_id.*                - Identifiant puce (chip serial) par plateforme
test/test_native/       - Tests unitaires (Unity)
docs/
  architecture.md       - Architecture logicielle
  configurations.md     - Index des configurations hardware
  components.md         - Inventaire composants disponibles
  mqtt-protocol.md      - Protocole MQTT device-side
  battery-cells.md      - Guide test/récup cellules 18650
  modules/              - Doc par module optionnel (user-facing, SVG)
  configs/              - Doc par configuration hardware (user-facing)
  datasheets/           - Datasheets composants (PDF)
  img/                  - Schémas SVG (wiring + schematic, light/dark)
```

## Ajouter un nouveau module

1. Créer `lib/thermo_core/src/modules/<name>_module.h/cpp` avec :
    - `<name>_module_register(ModuleRegistry&)` — ajoute metrics et/ou commandes
    - `<name>_module_contribute(PayloadBuilder&, ...)` — ajoute des champs au payload JSON
    - Optionnel : un `CommandHandler` pour les commandes spécifiques
2. Dans `src/main.cpp`, ajouter les blocs `#ifdef HAS_<NAME>` pour :
    - `#include` du module et du driver hw
    - Appel de `register` dans `register_modules()`
    - Appel de `contribute` dans `publish_sensor_data()`
3. Ajouter le flag `-DHAS_<NAME>` dans les envs concernés de `platformio.ini`
4. Ajouter les tests dans `test/test_native/`

## Conventions

- Code portable dans `lib/thermo_core/`, drivers capteurs dans `lib/thermo_drivers/`, code par plateforme dans `lib/thermo_platform/`
- Sélection plateforme via `#if defined(ESP8266)` / `#elif defined(ESP32)` /
  `#elif defined(ARDUINO_SAMD_MKRWIFI1010)` dans `main.cpp` et `config.h`
- `#ifdef NATIVE` / `#ifndef NATIVE` pour le code spécifique à la compilation locale
- `#ifdef HAS_xxx` pour le code conditionnel aux modules
- Tests unitaires : framework Unity dans `test/test_native/`
- Langue du code et commentaires : anglais
- Build flags : `-Wall -Wextra` activés sur toutes les cibles
- Tableaux Markdown : colonnes parfaitement alignées, séparateurs pleine largeur sans espaces (`|---|` et non `| --- |`)

## Protocole MQTT (sensor_server)

Le serveur récepteur est dans `../sensor_server/`. La spécification complète du protocole se trouve dans
`docs/mqtt-protocol.md` de ce projet.

### Topics

Tous les topics suivent le pattern `{device_type}/{device_id}/{message_type}` :

| Topic                      | Direction       | Exemple                                                                                           |
|----------------------------|-----------------|---------------------------------------------------------------------------------------------------|
| `thermo/{id}/sensors`      | Device → Server | `{"temp":22.5,"humi":45.2,"press":1013.1}`                                                        |
| `thermo/{id}/status`       | Device → Server | `{"level":"warning","message":"low_battery"}` ou `{"level":"error","message":"critical_battery"}` |
| `thermo/{id}/command`      | Server → Device | `{"action":"set_interval","value":30}`                                                            |
| `thermo/{id}/capabilities` | Device → Server | identité + metrics (`hwrev`/`ota`/`cal`), construit depuis le ModuleRegistry                      |
| `thermo/{id}/commands`     | Device → Server | liste commandes + params (réponse à `request_commands`)                                           |
| `thermo/{id}/calibration`  | Device → Server | rapport calibration `{"cal_temp":…,"cal_humi":…,"cal_press":…,"bat_divider":…}`                   |
| `thermo/{id}/ack`          | Device → Server | accusé de commande, ex. OTA `{"action":"ota_update","status":"start"}`                            |
| `thermo/{id}/diag`         | Device → Server | snapshot santé/technique `{"level":"warning","message":"missed_wakes","rst":4,"seq":842,…}`       |

### Règles critiques

1. **Pas de LWT online/offline** — Le serveur calcule le statut en ligne à partir de `last_seen` (offline si aucune
   donnée pendant 3× `publish_interval`). Ne pas utiliser de Last Will and Testament.
2. **Topic `command` (pas `config`)** — Le serveur envoie les commandes sur `{type}/{id}/command`.
3. **Format commandes** — `{"action":"...","value":...}`. Exemple : `{"action":"set_interval","value":300}`.
4. **Capabilities dynamiques** — Construit automatiquement depuis le `ModuleRegistry`. Chaque module enregistre ses
   metrics et commandes au démarrage. Découpé en deux messages (contrainte `MQTT_MAX_PACKET_SIZE=512`) : `capabilities`
   (identité + metrics + `hwrev`/`ota`/`cal`/`diag`) et `commands` (liste + params), ce dernier en réponse à
   `request_commands`. Le device répond à `request_capabilities` dans les 60 secondes. Les commandes cœur/OTA
   **toujours présentes** ne sont **pas** dans la liste `commands` (contrainte de taille) : le serveur les déduit des
   flags — `diag:1` → `get_status`/`get_diag`/`set_confirm_uplink`, `ota:1` → `ota_update`.
5. **Status = alertes JSON** — Format : `{"level":"...","message":"..."}`. Pas de online/offline.
6. **Auto-découverte** — Le serveur crée le device au premier message `sensors`. Données ignorées tant qu'un admin n'
   approuve pas.
7. **Validation serveur** — Payload max 10 KB, noms de métriques `^[a-zA-Z0-9_\-]+$` (max 64 chars), valeurs numériques
   uniquement.
8. **Diagnostics (toujours actif, sans flag, toutes plateformes)** — Le device évalue sa santé
   (`ok`/`info`/`warning`/`error`) à chaque réveil. Publie automatiquement sur `diag` uniquement si santé ≥ `warning`
   (les réveils nominaux ne publient rien de plus). `get_status` force un résumé sur `status`, `get_diag` force le
   snapshot sur `diag`. Annoncé via le flag `diag:1` des capabilities. Compteurs (`boot`/`seq`/`miss`/`pubfail`/`tx*`, + instrumentation
   chemin de réveil `wf`/`mf`/`sf`/`bx` émise si non-nulle) portables via la struct `DiagCounters` : ESP32
   RTC_DATA_ATTR, ESP8266 RTC user memory, SAMD/native RAM (standby).
   `set_confirm_uplink` (mesure de livraison uplink par bouclage broker, opt-in) marche sur toutes les plateformes.
   Voir `docs/diagnostics.md`.

### Commandes supportées

| Action                 | Payload                                       | Effet                                                               |
|------------------------|-----------------------------------------------|---------------------------------------------------------------------|
| `set_interval`         | `{"action":"set_interval","value":<seconds>}` | Change l'intervalle de publication (1-86400s)                       |
| `request_capabilities` | `{"action":"request_capabilities"}`           | Le device répond avec ses capabilities                              |
| `request_commands`     | `{"action":"request_commands"}`               | Le device répond avec la liste des commandes (`commands`)           |
| `get_status`           | `{"action":"get_status"}`                     | Le device répond avec un résumé santé sur `status`                  |
| `get_diag`             | `{"action":"get_diag"}`                       | Le device répond avec le snapshot technique sur `diag`              |
| `set_confirm_uplink`   | `{"action":"set_confirm_uplink","value":0/1}` | Active/désactive le diag de livraison uplink (`txok/txsent`, ESP32) |

Commandes ajoutées par les modules (voir tableau des modules et `docs/mqtt-protocol.md`) : `calibrate_battery`
(`HAS_BATTERY`), `set_offset` / `set_calibration` / `request_calibration` (`HAS_CALIBRATION`), `relay_toggle` /
`relay_contact` (`HAS_RELAY`), `ota_update` (`HAS_OTA`).
