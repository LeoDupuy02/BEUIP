# Projet BICEPS & BEUIP (BEUIP Version 1.0) (BICEPS Version 2.0)

Ce projet implémente un interpréteur de commandes (shell) personnalisé nommé **BICEPS**, couplé à un service de messagerie instantanée décentralisé nommé **BEUIP** basé surle protocole UDP.

## Composants

### 1. Le Shell BICEPS (`biceps.c`)
Le programme principal qui offre une interface utilisateur en ligne de commande.
* **Prompt Dynamique** : Génère un en-tête de type `user@hostname$` (ou `#` pour root) via la fonction `head()`.
* **Gestion de l'historique** : Utilise la bibliothèque `readline` pour sauvegarder et charger les commandes dans `hist_file`.
* **Analyseur de syntaxe** : La fonction `analyseCom` gère les séparateurs (espaces, tabulations) et permet l'exécution de commandes multiples séparées par `;`.

### 2. Gestionnaire de Commandes (`gescom.c`)
Assure la liaison entre l'utilisateur et le système ou le réseau.
* **Commandes Internes** : Implémente `exit`, `cd`, `pwd`, `vers`, et la passerelle `beuip`.
* **Communication IPC** : Lance le serveur de messagerie via un `fork()` et communique avec lui via deux **pipes** anonymes (`p1` pour l'entrée du serveur, `p2` pour la lecture des réponses).
* **Multiplexage** : Utilise `select()` avec un timeout pour lire de manière non-bloquante les messages provenant du serveur de messagerie (permet d'éviter les race conditions).

### 3. Serveur de Messagerie BEUIP (`servbeuip.c` & `creme.c`)
Le moteur de communication réseau décentralisé.
* **Découverte Réseau** : Envoie un message de **Broadcast UDP** au démarrage pour signaler sa présence aux autres machines.
* **Table de Voisins** : Maintient une liste dynamique (`network`) des utilisateurs connectés (IP et pseudo).
* **Stockage Local** : Mémorise les messages entrants dans une pile circulaire (`memorize`) pour une consultation ultérieure via BEUIP.

---

## BEUIP : Protocole de Communication (Types de messages)

Le protocole utilise le premier octet du datagramme pour identifier l'action :

| Code | Type | Description |
| :--- | :--- | :--- |
| **0** | **EXIT** | Notifie le réseau que l'utilisateur se déconnecte. |
| **1** | **HELLO** | Message de broadcast pour découvrir les voisins. |
| **2** | **ACK** | Réponse à un HELLO pour confirmer sa présence. |
| **3** | **LIST** | (Local) Demande au serveur d'afficher les clients connectés. |
| **4** | **SEND** | (Local) Demande l'envoi d'un message privé à un pseudo spécifique. |
| **5** | **ALL** | (Local) Envoi d'un message à tous les utilisateurs (Broadcast dirigé). |
| **6** | **LOG** | (Local) Affiche et vide la pile des messages reçus. |
| **9** | **MSG** | Message texte entrant provenant d'un autre utilisateur. |


## Guide d'Utilisation

### Compilation
Le projet nécessite la bibliothèque `libreadline` :
```bash
sudo apt install libreadline-dev
```

La compilation se fait avec les Makefile.

### Objdump / nm
Vérification de la table des symboles :

```bash
objdump -t biceps
nm biceps.o
nm gescom.o
```

## Valgrind

```bash
  valgrind --leak-check=yes myprog arg1 arg2
```

## Captures d'écran
Les captures d'écran "valgrind" et "ps" permettent de voir la bonne terminaison du shell : pas de perte de mémoire et de processus zombie.

### Notes de développement
* Buffering : Le serveur servbeuip utilise fflush(stdout) après chaque affichage pour garantir que les messages remontent correctement dans le pipe vers biceps.
* Broadcast : Le service utilise l'option de socket SO_BROADCAST pour permettre la découverte automatique sur le sous-réseau.
