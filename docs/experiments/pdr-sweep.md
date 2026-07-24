# Protocole PDR-sweep — chiffrer le déficit TX de l'ESP32-C3 vs ESP8266

Status: **protocole (non exécuté)**. Objectif : mettre un **chiffre** sur l'hypothèse « la perte
de trames du C3 est un déficit d'émission (TX) RF », en mesurant le **taux de livraison de paquets
(PDR = Packet Delivery Ratio)** en fonction de l'**atténuation du lien**, comparativement entre un
C3 et un 8266 placés dans des conditions identiques. Voir le contexte dans `docs/diagnostics.md`
(mesure de livraison uplink) et la synthèse de recherche (mémoire `project_c3_transmission_loss`).

## Idée directrice

Le RSSI côté serveur ne mesure que la **réception** du device (downlink). Il ne voit pas la
qualité d'**émission** (uplink) du device. On contourne ça en mesurant directement combien de
**publishes arrivent réellement au broker** (PDR), et comment ce PDR **s'effondre quand on dégrade
le lien**. Si le C3 décroche à une atténuation bien plus faible que le 8266 **alors qu'il « entend »
encore bien l'AP** (RSSI RX correct), c'est la preuve chiffrée d'un déficit TX.

## Métrique (déjà instrumentée, aucun matériel requis)

Le PDR se lit **sans accès à l'AP** grâce aux compteurs déjà en place :

- **`txok / txsent`** (mode `set_confirm_uplink=1`) : confirmation par bouclage broker, remontée
  dans le `diag` à chaque réveil. PDR = `Δtxok / Δtxsent` sur la fenêtre.
- **Ou** perte de cadence : gaps dans la série `rssi`/`temp` côté API (`Δseq` vs trames reçues).
- **Facteur de confiance** : viser **≥ 200 trames par palier** (n=200 → IC ~±5 % à 90 % de PDR).
  À `publish_interval=10 s` (build debug, sur USB) → ~33 min/palier. À 300 s → ne pas utiliser
  pour un sweep (trop lent) ; passer les nodes de test à **10–30 s** via `set_interval`.

## Contrôle de l'atténuation (du plus simple au plus rigoureux)

1. **Distance croissante** (le plus simple) : positions fixes repérées (ex. 1 m, 5 m, 10 m, 15 m,
   +1 mur, +2 murs). Peu reproductible (multipath) mais suffisant pour une **grande** différence.
2. **Atténuation par obstacle calibré** : éloignement identique + caisson métallique / feuille
   alu partielle / éloignement derrière béton. Grossier.
3. **Atténuateur SMA** (le plus propre, si carte à u.FL) : atténuateurs 3/6/10/20 dB en série sur
   la sortie antenne. Nécessite une carte C3 avec connecteur u.FL (pas le Super Mini céramique).

> Le **point clé anti-biais** : à chaque palier, mesurer **les deux plateformes dans la MÊME
> position**, idéalement en **échangeant leurs emplacements** (A/B) pour annuler les effets de
> point chaud. Les deux tournent le **même intervalle**, `confirm-mode ON`, même AP.

## Déroulé

1. **Paire de test co-localisée** : 1× C3 (`c3test`, antenne céramique — et/ou `thermo_3` ¼λ) et
   1× ESP8266 (un D1 Mini de test), sur l'AP habituel. `set_interval 15` sur les deux (USB → pas
   de contrainte batterie). `set_confirm_uplink 1` sur les deux.
2. Pour chaque **palier d'atténuation** (position P0 la plus proche → Pn la plus lointaine) :
   - Placer les deux nodes en P_i (côte à côte, < 20 cm), noter l'heure UTC de début.
   - Laisser tourner **≥ 200 trames** (~50 min à 15 s, ou baisser à 10 s).
   - **Échanger les positions** des deux nodes et refaire ≥ 200 trames (annule le point chaud).
   - Noter l'heure UTC de fin.
3. **Extraction** (API read-only, fenêtre = [début, fin] du palier, comme les scripts de bilan) :
   pour chaque node et chaque palier → **PDR** (via `txok/txsent` ou cadence) + **RSSI moyen** (RX).
4. **Courbe** : PDR (axe Y) vs palier / RSSI RX (axe X), une série par plateforme.

## Lecture du résultat

| Observation                                                                 | Conclusion                                            |
|-----------------------------------------------------------------------------|-------------------------------------------------------|
| Le C3 chute sous ~95 % de PDR à un **RSSI RX bien meilleur** que le 8266    | **Déficit TX du C3 confirmé et chiffré** (asymétrie)  |
| C3 et 8266 chutent au même RSSI RX                                          | Lien symétrique — la perte n'est pas propre au C3     |
| `thermo_3` (¼λ) décroche entre `c3test` (céramique) et le 8266              | Le mod ¼λ récupère une partie du déficit, quantifiée  |

Grandeur attendue (d'après la recherche) : le C3 devrait perdre le lien **~6–10 dB plus tôt** que
le 8266 (déficit de rayonnement TX de l'antenne). Chiffrer ce Δ (en dB de RSSI RX au point de
décrochage, ou en paliers) **est** le livrable.

## Variante `TxPower` (isole matching RF vs puissance)

À **position fixe** (un palier où le C3 perd déjà ~10–20 %), balayer `WiFi.setTxPower()` du C3
(ex. 8,5 / 11 / 15 / 17 / 19 dBm) et mesurer le PDR à chaque niveau :

- PDR **plat** quand la puissance monte → lien **limité par l'adaptation d'antenne** (matching /
  rayonnement) → c'est bien le **hardware RF** (cohérent avec l'antenne mal adaptée).
- PDR qui **monte** avec la puissance → lien limité par la **puissance** → marge TX insuffisante.

## Complément (si l'AP est Linux/OpenWrt) — étalon-or

Sur l'AP : `iw dev <wlan> station dump` par MAC → `tx bitrate`, `tx retries`, `tx failed`,
`signal`. À position/atténuation **identique**, un `tx failed`/`tx retries` **plus élevé sur le
C3 à `signal` (RX) égal** = preuve directe côté infrastructure du déficit d'émission. C'est la
mesure la plus fiable ; les méthodes PDR ci-dessus la reproduisent sans accès à l'AP.

## Limites

- RSSI RX on-chip **non calibré** (relatif, pas absolu) — l'analyse est **comparative** C3-vs-8266,
  pas une métrologie RF absolue.
- Multipath / variabilité de position → d'où l'A/B et les ≥ 200 trames/palier.
- Variance **batch** des Super Mini : tester plusieurs exemplaires si possible.
- Un PDR imputé à la TX suppose que la connexion (association) réussit ; logguer aussi `miss`
  (échecs de connexion) pour ne pas confondre « connexion ratée » et « publish perdu en vol ».
