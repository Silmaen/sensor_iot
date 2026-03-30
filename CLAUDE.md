# Thermo - Projet IoT Thermostat

## Aperçu

Projet PlatformIO modulaire multi-plateforme pour des capteurs IoT. Supporte ESP8266 (D1 Mini) et SAMD21 (Arduino MKR
WiFi 1010). Architecture basée sur des feature flags (`HAS_xxx`) permettant de composer des variantes hardware à partir
d'un noyau MQTT commun.

## Architecture modulaire

Le projet s'articule autour de :

- **Noyau MQTT** (`src/main.cpp`) : connexion, publication, commandes, capabilities — toujours présent
- **Modules optionnels** activés par feature flags dans `platformio.ini` :

| Flag               | Module                        | Metrics ajoutées                      | Commandes ajoutées |
|--------------------|-------------------------------|---------------------------------------|--------------------|
| `HAS_BME280`       | Capteur BME/BMP280            | `temperature`, `humidity`, `pressure` | —                  |
| `HAS_SHT30`        | Capteur SHT30 (shield Wemos)  | `temperature`, `humidity`             | —                  |
| `HAS_MKR_ENV`      | MKR ENV Shield (4 capteurs)     | `temperature`, `humidity`, `pressure`, `light_lux`, `uv_index` | — |
| `HAS_BATTERY`      | Monitoring batterie           | `battery_pct`, `battery_v`            | —                  |
| `HAS_DISPLAY`      | Afficheur 7-segments + bouton | —                                     | —                  |
| `HAS_DEEP_SLEEP`   | Deep sleep entre lectures     | —                                     | —                  |
| `HAS_SERIAL_DEBUG` | Logs debug verbose sur série  | —                                     | —                  |

> **Note** : `HAS_BME280`, `HAS_SHT30` et `HAS_MKR_ENV` sont mutuellement exclusifs.

- **Identité device** définie par `-DDEVICE_ID` et `-DMQTT_DEVICE_TYPE` dans `platformio.ini`

## Environnements PlatformIO

```bash
# ESP8266 (D1 Mini)
pio run -e sensor_8266_empty          # Build : BME280 + debug (dev)
pio run -e sensor_8266_bmp80          # Build : BME280 + batterie + deep sleep
pio run -e sensor_8266_sht30          # Build : SHT30 + batterie + deep sleep
pio run -e sensor_8266_display_sht30  # Build : SHT30 + afficheur + debug
pio run -e sensor_8266_bmp80 -t upload  # Upload ESP8266

# SAMD21 (Arduino MKR WiFi 1010 + MKR ENV Shield)
pio run -e sensor_mkr_env             # Build : MKR ENV Shield + batterie 1S (duty-cycle)
pio run -e sensor_mkr_env -t upload   # Upload MKR

# Utilitaires
pio run -e cell_tester_mkr            # Build : testeur de cellules 18650 (MKR, série)
pio run -e cell_tester_mkr -t upload  # Upload testeur

# Tests & monitoring
pio run -e native              # Build natif (tests)
pio test -e native             # Exécuter les tests unitaires (63 tests)
pio device monitor             # Moniteur série (115200 baud)
```

**Note :** L'intervalle de publication par défaut est 10s avec `HAS_SERIAL_DEBUG` (dev), 300s (5 min)
sans (production). Configurable à distance via la commande MQTT `set_interval`.

## Structure

```
src/
  main.cpp              - Orchestrateur modulaire (#ifdef HAS_xxx)
  cell_tester.cpp       - Firmware testeur de cellules 18650 (MKR, standalone)
  hw/                   - Drivers hardware (ESP8266 + SAMD21)
include/
  config.h              - Pins, timing, topics MQTT (DEVICE_ID/TYPE via -D flags)
  credentials.h         - WiFi & MQTT credentials (gitignored)
  interfaces/           - Interfaces abstraites
lib/thermo_core/        - Bibliothèque portable et testable
  src/
    module_registry.h/cpp    - Registre de modules (metrics, commands, handlers)
    payload_builder.h/cpp    - Construction JSON incrémentale
    mqtt_payload.h/cpp       - Formatage payloads & parsing commandes
    modules/
      bme280_module.h/cpp    - Module BME280 (register + contribute)
      sht30_module.h/cpp     - Module SHT30 (register + contribute)
      mkr_env_module.h/cpp   - Module MKR ENV Shield (register + contribute)
      battery_module.h/cpp   - Module batterie (register + contribute)
    sensor_data.h, battery.h/cpp, display_encoding.h/cpp
test/test_native/       - Tests unitaires (Unity)
docs/
  architecture.md       - Architecture logicielle
  configurations.md     - Index des configurations hardware
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

- Code portable dans `lib/thermo_core/`, code hardware-spécifique dans `src/hw/`
- Sélection plateforme via `#if defined(ESP8266)` / `#elif defined(ARDUINO_SAMD_MKRWIFI1010)` dans `main.cpp` et `config.h`
- `#ifdef NATIVE` / `#ifndef NATIVE` pour le code spécifique à la compilation locale
- `#ifdef HAS_xxx` pour le code conditionnel aux modules
- Tests unitaires : framework Unity dans `test/test_native/`
- Langue du code et commentaires : anglais
- Build flags : `-Wall -Wextra` activés sur toutes les cibles

## Protocole MQTT (sensor_server)

Le serveur récepteur est dans `../sensor_server/`. La spécification complète du protocole se trouve dans
`docs/mqtt-protocol.md` de ce projet.

### Topics

Tous les topics suivent le pattern `{device_type}/{device_id}/{message_type}` :

| Topic                      | Direction       | Exemple                                                  |
|----------------------------|-----------------|----------------------------------------------------------|
| `thermo/{id}/sensors`      | Device → Server | `{"temperature":22.5,"humidity":45.2,"pressure":1013.1}` |
| `thermo/{id}/status`       | Device → Server | `{"level":"warning","message":"low_battery"}` ou `{"level":"error","message":"critical_battery"}` |
| `thermo/{id}/command`      | Server → Device | `{"action":"set_interval","value":30}`                   |
| `thermo/{id}/capabilities` | Device → Server | construit dynamiquement depuis le ModuleRegistry         |

### Règles critiques

1. **Pas de LWT online/offline** — Le serveur calcule le statut en ligne à partir de `last_seen` (offline si aucune
   donnée pendant 3× `publish_interval`). Ne pas utiliser de Last Will and Testament.
2. **Topic `command` (pas `config`)** — Le serveur envoie les commandes sur `{type}/{id}/command`.
3. **Format commandes** — `{"action":"...","value":...}`. Exemple : `{"action":"set_interval","value":300}`.
4. **Capabilities dynamiques** — Le message capabilities est construit automatiquement depuis le `ModuleRegistry`.
   Chaque module enregistre ses metrics et commandes au démarrage. Le device répond à `request_capabilities` dans les 60
   secondes.
5. **Status = alertes JSON** — Format : `{"level":"...","message":"..."}`. Pas de online/offline.
6. **Auto-découverte** — Le serveur crée le device au premier message `sensors`. Données ignorées tant qu'un admin n'
   approuve pas.
7. **Validation serveur** — Payload max 10 KB, noms de métriques `^[a-zA-Z0-9_\-]+$` (max 64 chars), valeurs numériques
   uniquement.

### Commandes supportées

| Action                 | Payload                                       | Effet                                         |
|------------------------|-----------------------------------------------|-----------------------------------------------|
| `set_interval`         | `{"action":"set_interval","value":<seconds>}` | Change l'intervalle de publication (1-86400s) |
| `request_capabilities` | `{"action":"request_capabilities"}`           | Le device doit répondre avec ses capabilities |
