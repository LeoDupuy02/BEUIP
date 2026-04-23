NOM: DUPUY
PRENOM: Leo

# Projet BICEPS & BEUIP (Version 2.3 - Rendu TP3)

Ce projet implémente un interpréteur de commandes (shell) personnalisé nommé **BICEPS**, couplé à un service de messagerie instantanée décentralisé nommé **BEUIP** basé sur le protocole UDP. 

Cette version 2.3 abandonne l'architecture multi-processus au profit du **multi-threading**, permettant un partage sécurisé des données en mémoire via des mutex.

## Composants de l'Architecture

### 1. Le Shell BICEPS (`biceps.c`)
Le programme principal qui offre l'interface utilisateur en ligne de commande.
* **Prompt Dynamique** : Génère un en-tête de type `user@hostname$` (ou `#` pour root) via la fonction `head()`, avec une gestion stricte de la mémoire allouée.
* **Gestion de l'historique** : Utilise la bibliothèque `readline` pour sauvegarder et charger les commandes dans `hist_file`.
* **Analyseur de syntaxe** : La fonction `analyseCom` gère les séparateurs (espaces, tabulations) et permet l'exécution de commandes multiples séparées par `;`.

### 2. Gestionnaire de Commandes (`gescom.c`)
Assure la liaison entre l'utilisateur et le système ou le réseau.
* **Commandes Internes** : Implémente `exit`, `cd`, `pwd`, `vers`, et la passerelle `beuip`.
* **Multi-threading** : Au lieu d'utiliser `fork()`, la commande `beuip start` lance le serveur UDP dans un thread dédié via `pthread_create()`. L'arrêt se fait proprement via `pthread_cancel()` et `pthread_join()`.

### 3. Serveur de Messagerie BEUIP (`creme.c`)
Le moteur de communication réseau décentralisé, tournant en tâche de fond.
* **Découverte Réseau** : Envoie un message de **Broadcast UDP** au démarrage sur l'adresse définie par `BROADCAST_IP` pour signaler sa présence aux autres machines.
* **Liste Chaînée Dynamique** : Maintient une liste dynamique et *triée par ordre alphabétique* des utilisateurs connectés (IP et pseudo), remplaçant l'ancien tableau statique.
* **Accès Concurrent (Mutex)** : Toute lecture ou modification de la liste chaînée par le shell (ex: `beuip list`) ou par le serveur réseau (ex: réception d'un départ) est protégée par `pthread_mutex_t` pour éviter les accès concurrents.

---

## BEUIP : Protocole de Communication (Types de messages)

Le protocole utilise le premier octet du datagramme pour identifier l'action réseau :

| Code | Type | Description |
| :--- | :--- | :--- |
| **0** | **EXIT** | Notifie le réseau que l'utilisateur se déconnecte (Broadcast sans AR). |
| **1** | **HELLO** | Message de broadcast pour découvrir les voisins. |
| **2** | **ACK** | Réponse à un HELLO pour confirmer sa présence. |
| **9** | **MSG** | Message texte entrant provenant d'un autre utilisateur. |

*Note : Les codes 3, 4, 5 et 6 des versions précédentes (qui utilisaient des sockets locales 127.0.0.1) ont été supprimés. Ces actions sont désormais exécutées directement en mémoire partagée grâce aux threads.*

---

## Guide d'Utilisation

### Compilation et Nettoyage
Le projet requiert la bibliothèque `libreadline-dev`. La compilation est stricte (`-Wall -Werror`).

```bash
# Tout compiler pour générer l'exécutable "biceps"
make

# Nettoyer les fichiers objets et les exécutables
make clean