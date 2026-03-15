# Thermo - Projet IoT Thermostat

## Aperçu
Projet PlatformIO multi-cibles pour un thermostat IoT.

## Cibles
- **micro** : Arduino Micro (ATmega32U4)
- **esp8266** : ESP-12E (ESP8266)
- **esp32** : ESP32 Dev Module
- **native** : Compilation locale pour debug et tests unitaires

## Structure
```
src/         - Code source principal
include/     - Headers partagés
lib/         - Bibliothèques privées du projet
test/        - Tests (test_native/ pour les tests locaux)
```

## Commandes essentielles
```bash
pio run                        # Build toutes les cibles hardware
pio run -e native              # Build natif (local)
pio run -e esp32               # Build une cible spécifique
pio test -e native             # Exécuter les tests unitaires
pio run -e esp32 -t upload     # Upload sur la carte
pio device monitor             # Moniteur série (115200 baud)
```

## Conventions
- Le code portable va dans `src/` et `include/`, gardé indépendant de la plateforme autant que possible
- Utiliser `#ifdef NATIVE` / `#ifndef NATIVE` pour le code spécifique à la compilation locale
- Les tests unitaires utilisent le framework Unity et se trouvent dans `test/test_native/`
- Le code spécifique à une plateforme doit utiliser les guards appropriés (`#ifdef ESP32`, `#ifdef ESP8266`, etc.)
- Langue du code et commentaires : anglais
- Build flags : `-Wall -Wextra` activés sur toutes les cibles
