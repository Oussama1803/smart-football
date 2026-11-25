# Tableau de bord télémétrique Smart Football (Étape 6)

Ce composant fournit l'interface visuelle temps réel pour visualiser les tirs, analyser les performances des joueurs, comparer les séances d'entraînement et superviser les dispositifs connectés (ESP32 du but et extension gardien).

---

## 1. Philosophie de conception & Choix techniques

Conformément aux directives de l'équipe :
- **Technologies utilisées** : HTML5 sémantique, CSS3 moderne (CSS Grid, Flexbox, variables CSS, thème sombre athlétique) et JavaScript moderne (ES6+ Vanilla).
- **Zéro dépendance de compilation externe** : Aucun framework lourd (React/Angular/Vue), aucun bundler (Webpack/Vite), aucun `node_modules`.
- **Avantages pour le projet** :
  - Empreinte mémoire minimale (< 5 Mo de RAM) pour le serveur web Nginx sur le VPS.
  - Temps de chargement ultra-rapide sans latence de démarrage.
  - Déploiement immédiat par simple montage de volume ou service de fichiers statiques Nginx.
  - Code lisible et directement modifiable sans recompilation.

---

## 2. Structure des fichiers

```text
frontend/
├── index.html       # Structure sémantique du dashboard, but interactif, cartes KPIs et tableaux
├── css/
│   └── styles.css   # Feuille de style complète, thème sombre moderne, mise en page réactive
└── js/
    └── app.js       # Logique applicative, polling API, calculs visuels de heatmap et comparaisons
```

---

## 3. Fonctionnalités détaillées

### 3.1 Indicateurs clés de performance (KPIs)
Le haut de l'interface affiche les métriques synthétiques globales ou filtrées par séance :
- **Total de tirs** : volume global de la séance.
- **Taux de précision (%)** : calculé selon la règle convenue :
  $$\text{Taux de précision} = \frac{\text{Tirs cadrés}}{\text{Total des tirs}} \times 100$$
  où un tir sans impact dans la fenêtre est consigné avec `target_zone = "none"` (tir non cadré).
- **Vitesse moyenne et maximale** (km/h) : mesurée par les barrières optiques infrarouges.
- **Pression relative moyenne (FSR)** : valeur brute ADC [0-4095].
- **Intensité relative d'impact (Piézo)** : valeur brute ADC [0-4095].

### 3.2 Représentation interactive du but (Heatmap)
- **Cadre dynamique à 4 quadrants** :
  - `upper_left` (Haut gauche)
  - `upper_right` (Haut droit)
  - `lower_left` (Bas gauche)
  - `lower_right` (Bas droit)
- **Zone Hors Cadre** : Bandeau dédié affichant le nombre et le pourcentage de tirs manqués (`target_zone = "none"`).
- **Extension 9 zones** : Bouton de bascule dans l'interface permettant de subdiviser le but en 9 zones (`upper_center`, `lower_center`, `center`, `middle_left`, `middle_right`, etc.) sans rupture d'intégration.
- **Visualisation de l'intensité** : Les zones changent dynamiquement de teinte et d'opacité selon la concentration de tirs enregistrés.

### 3.3 Historique chronologique des tirs
- Tableau affichant chaque tir horodaté avec son identifiant UUID unique.
- Visualisation de la vitesse (avec code couleur de puissance), de la zone touchée, de la pression brute FSR et de la latence de réception serveur ($\Delta t$).

### 3.4 Outil de comparaison de séances
- Deux menus déroulants permettent de choisir deux séances distinctes (ex. `session-001` vs `session-002`).
- Affichage comparatif côte à côte avec différentiels automatiques (gain ou perte de vitesse, amélioration du taux de précision cadré, volume de tirs).

### 3.5 Extension Gardien de but (`keeper-esp32-01`)
- Carte de supervision spécifique affichant la disponibilité et l'activité du boîtier gardien équipé de la centrale inertielle MPU6050 (accéléromètre et gyroscope 6 axes).

### 3.6 Transparence et intégrité de la donnée
Le tableau de bord applique rigoureusement la règle du projet distinguant les types de données via un badge et des infobulles explicites :
- 🟢 **Mesure réelle** : vitesse calculée par les barrières IR ($v = D / \Delta t$) et zone d'impact piézo.
- 🟡 **Valeur brute ADC relative** : pression FSR et intensité piézo (exprimées en unités brutes relatives [0-4095], jamais en Newtons).
- 🔵 **Données de démonstration** : simulation générée dans Wokwi ou via le simulateur de tests.

---

## 4. Intégration avec l'API REST

Le tableau de bord interroge l'API backend à intervalle régulier (polling toutes les 3 secondes) :
- `GET /health` : contrôle de la disponibilité du serveur et affichage du voyant de connexion vert/rouge.
- `GET /api/stats?session_id=...` : actualisation des indicateurs synthétiques et de la répartition des zones du but.
- `GET /api/shots?limit=20` : récupération des derniers tirs pour le tableau d'historique.
- `GET /api/sessions` : mise à jour de la liste des séances disponibles dans les sélecteurs.
- `GET /api/devices` : supervision de l'état des ESP32 connectés.
- `GET /api/stats/compare?session_a=...&session_b=...` : calcul de la comparaison entre deux entraînements.

---

## 5. Visualisation locale

Le tableau de bord peut être visualisé de trois façons :

1. **Via le conteneur Nginx (recommandé en production/simulation complète)** :
   ```bash
   cd infra && docker compose up -d
   # Ouvrir http://localhost/ dans votre navigateur
   ```

2. **Directement via le serveur FastAPI** (qui monte `frontend/` comme dossier statique) :
   ```bash
   cd backend && uvicorn app.main:app --port 8000
   # Ouvrir http://localhost:8000/ dans votre navigateur
   ```

3. **Via un simple serveur HTTP Python** :
   ```bash
   python3 -m http.server 3000 --directory frontend
   # Ouvrir http://localhost:3000/
   ```
