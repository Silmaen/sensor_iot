# Thermo - Projet IoT Thermostat

## Aperçu
Projet PlatformIO multi-cibles pour un thermostat IoT.

## Cibles
- **esp8266** : Wemos D1 Mini v3.0.0 (ESP8266)
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
pio run -e esp8266             # Build ESP8266
pio run -e native              # Build natif (local)
pio test -e native             # Exécuter les tests unitaires
pio run -e esp8266 -t upload   # Upload sur la carte
pio device monitor             # Moniteur série (115200 baud)
```

## Conventions
- Le code portable va dans `src/` et `include/`, gardé indépendant de la plateforme autant que possible
- Utiliser `#ifdef NATIVE` / `#ifndef NATIVE` pour le code spécifique à la compilation locale
- Les tests unitaires utilisent le framework Unity et se trouvent dans `test/test_native/`
- Le code spécifique à la plateforme ESP8266 doit utiliser `#ifdef ESP8266`
- Langue du code et commentaires : anglais
- Build flags : `-Wall -Wextra` activés sur toutes les cibles
