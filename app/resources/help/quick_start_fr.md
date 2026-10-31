# Guide de demarrage rapide

AI File Sorter vous aide à organiser vos fichiers en commençant par une vérification. L'approbation automatique peut présélectionner les changements dans la liste de vérification.

L'IA propose des catégories, des dossiers de destination et des noms. L'application déplace ou renomme les fichiers uniquement après votre confirmation des changements sélectionnés dans la fenêtre de vérification.

## Premiere execution sure

Si c'est votre premiere utilisation d'AI File Sorter, commencez avec un petit dossier de test avant de choisir une grande archive ou un disque entier.

Bons dossiers pour une premiere execution :

- une copie de 20-50 fichiers depuis `Downloads`
- un petit dossier de captures d'ecran ou de photos a nettoyer
- un dossier temporaire avec quelques PDF ou documents

Cela rend la premiere execution facile a verifier. Vos fichiers restent sur votre ordinateur lorsque vous utilisez des modeles locaux, et l'IA ne fait que proposer des categories et des noms. Laissez les options d'approbation automatique desactivees pour votre premiere execution afin que rien ne soit deplace ou renomme avant votre approbation de la liste de verification.

## Configurer un LLM

Ouvrez **Settings -> Select LLM**. Choisissez un modèle dans la liste déroulante **Local LLMs** ou un fournisseur dans **API connections**.

**Qwen3.5 4B (Recommended)** est la première option locale et le modèle par défaut des nouvelles configurations. Il offre une grande précision de catégorisation et est recommandé pour les structures de dossiers existantes. Votre choix de modèle déjà enregistré est conservé. Pour utiliser votre propre fichier GGUF, choisissez **Custom local LLM (gguf)** puis **Add**.

Pour un modèle local proposé par l'application, développez **Downloads**, cliquez sur **Download** et attendez **Model ready** avant de confirmer avec **OK**. **Model storage** permet de choisir le dossier de téléchargement. **See details** affiche l'URL de téléchargement, le chemin local et les informations techniques.

Pour l'analyse d'images, choisissez aussi un **Visual model** dans **Image analysis models** et téléchargez tous ses fichiers requis. Ce choix est distinct du modèle de texte.

Dans **API connections**, choisissez Gemini, ChatGPT (OpenAI) ou un point d'accès personnalisé compatible OpenAI, puis saisissez les paramètres de connexion requis et le nom du modèle. Les API cloud nécessitent Internet et envoient les données d'analyse au service configuré. Les points d'accès personnalisés peuvent aussi fonctionner localement.

## Vérifier les performances locales (facultatif)

Après avoir téléchargé les modèles, ouvrez **File -> System compatibility check** et cliquez sur **Run benchmark**. Fermez d'abord les autres applications qui sollicitent fortement le processeur ou le GPU. La vérification teste les modèles locaux téléchargés, y compris les modèles GGUF personnalisés.

Laissez **Backend** sur **Automatic (try CPU if GPU fails)** ou choisissez **All available backends** pour comparer le processeur et les options GPU prises en charge. Les résultats sont enregistrés séparément pour chaque modèle et moteur ; un échec sur l'un n'exclut pas une combinaison fonctionnelle sur un autre.

Sélectionnez un résultat et utilisez **Retry selected** pour retester cette combinaison, **Retry failed** pour retester les combinaisons en échec ou **Try chosen backend** pour tester le modèle sélectionné avec un moteur précis de la liste déroulante.

Les recommandations estiment les performances locales. La vérification ne teste ni les services API ni la précision du choix des dossiers existants, et ne change pas automatiquement votre modèle sélectionné.

## 1. Choisir un dossier

Utilisez **Browse** ou le panneau **File Explorer** pour choisir le dossier a trier.

Exemples typiques :

- `Downloads`
- un dossier de nettoyage du bureau
- un dossier sur un disque externe
- un dossier reseau ou synchronise avec le cloud
- une archive de projet

**Destination** détermine où vont les fichiers triés. Laissez **Use analyzed folder** activé pour trier dans le dossier sélectionné, ou désactivez-le et choisissez une autre destination.

## 2. Choisir ce que l'application doit faire

Utilisez les options principales pour definir si l'application doit :

- classer les fichiers dans des dossiers par categorie
- analyser les images
- analyser les documents
- proposer des suggestions de renommage pour les fichiers pris en charge

Si vous voulez seulement des suggestions de renommage, activez le mode correspondant.

## 3. Choisir comment trier

Choisissez un mode de tri :

- **Create category folders** crée des dossiers à partir des noms de catégories proposés.
- **Use existing folder structure** choisit des dossiers dans l'arborescence de votre **Destination**. Activez **Suggest new folders when needed** pour autoriser de nouvelles destinations si aucune ne convient ; laissez cette option désactivée pour utiliser uniquement les dossiers existants.

Pour les dossiers existants, nous recommandons **Qwen3.5 4B**. **Gemma 1.1 7B** et les modèles accessibles par API sont aussi des options adaptées. Les autres modèles locaux affichent un conseil ; vous pouvez conserver votre modèle ou le changer dans **Settings -> Select LLM**.

En mode **Create category folders**, choisissez un style de catégorisation :

- **More refined** pour un usage general et un classement plus detaille
- **More consistent** si vous voulez une meilleure coherence entre fichiers similaires

Vous pouvez aussi activer les listes blanches de categories pour limiter les noms de categories autorises.

## 4. Lancer l'analyse

Cliquez sur **Analyze folder**.

L'application analyse le dossier selectionne, rassemble les informations necessaires et prepare une liste de verification.

## 5. Verifier avant d'appliquer

La fenetre de verification vous permet d'examiner :

- les categories proposees
- les sous-categories optionnelles
- les suggestions de renommage pour les fichiers pris en charge
- les chemins de destination finaux
- les apercus de fichiers lorsque c'est pris en charge

Vous pouvez ajuster ou refuser les suggestions avant de confirmer quoi que ce soit.

Activez **Dry run (preview only, do not move files)** pour prévisualiser les déplacements et renommages prévus sans les appliquer.

## 6. Appliquer les modifications

Sélectionnez les lignes à traiter et cliquez sur **Confirm and Process**. L'application crée les dossiers nécessaires et effectue les déplacements ou renommages sélectionnés.

**Auto-approve filename changes** et **Auto-approve categorization** présélectionnent les lignes admissibles. Vous devez toujours confirmer avant l'application de ces changements.

## 7. Annuler la derniere execution

Si vous appliquez des modifications puis voulez les annuler, utilisez **Edit -> Undo last run** dans le menu.

L'annulation est prevue pour la derniere execution de tri confirmee. Elle utilise l'historique enregistre par l'application pour remettre les fichiers a leur emplacement precedent et annuler les renommages pris en charge lorsque c'est possible.

Pour de meilleurs resultats, utilisez l'annulation avant de lancer un autre grand nettoyage dans le meme dossier.

## 8. Apprentissage a partir de vos validations

Quand vous approuvez des categories dans la fenetre de verification, l'application peut memoriser ces decisions locales et les utiliser comme indices lors des prochaines executions. Cela n'entraine pas et ne modifie pas le modele d'IA.

Les exemples appris sont stockes dans une base de donnees locale separee. Vider le cache de categorisation normal ne les supprime donc pas. Pour supprimer ces donnees d'apprentissage locales, utilisez **Settings -> Reset learned behavior**.

## Bon a savoir

- L'application utilise un cache local pour eviter de retraiter les memes fichiers et pour ameliorer la coherence.
- L'approbation automatique est optionnelle. Laissez-la desactivee jusqu'a ce que les suggestions de l'application vous conviennent.
- Les options d'image et de document peuvent etre developpees separement si vous avez besoin de plus de controle.

## Si quelque chose semble incorrect

Verifiez d'abord les points suivants :

- le dossier selectionne est bien celui que vous vouliez
- les options d'analyse pertinentes sont activees
- le mode renommage uniquement ne limite pas le resultat d'une maniere inattendue
- une liste blanche de categories ne restreint pas trop les suggestions

Pour plus d'aide, ouvrez **Help -> FAQ**.
