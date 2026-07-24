# Guide de demarrage rapide

AI File Sorter vous aide a organiser vos fichiers avec un flux de travail qui commence par la verification et une approbation automatique optionnelle pour les changements auxquels vous choisissez de faire confiance.

L'IA pilote l'analyse et propose des categories, des sous-categories et des noms. Elle ne modifie pas directement vos fichiers. L'application effectue les deplacements ou renommages apres votre confirmation des changements verifies, ou automatiquement seulement si vous avez active l'option d'approbation automatique correspondante.

## Premiere execution sure

Si c'est votre premiere utilisation d'AI File Sorter, commencez avec un petit dossier de test avant de choisir une grande archive ou un disque entier.

Bons dossiers pour une premiere execution :

- une copie de 20-50 fichiers depuis `Downloads`
- un petit dossier de captures d'ecran ou de photos a nettoyer
- un dossier temporaire avec quelques PDF ou documents

Cela rend la premiere execution facile a verifier. Vos fichiers restent sur votre ordinateur lorsque vous utilisez des modeles locaux, et l'IA ne fait que proposer des categories et des noms. Laissez les options d'approbation automatique desactivees pour votre premiere execution afin que rien ne soit deplace ou renomme avant votre approbation de la liste de verification.

## 1. Choisir un dossier

Utilisez **Browse** ou le panneau **File Explorer** pour choisir le dossier a trier.

Exemples typiques :

- `Downloads`
- un dossier de nettoyage du bureau
- un dossier sur un disque externe
- un dossier reseau ou synchronise avec le cloud
- une archive de projet

## 2. Choisir ce que l'application doit faire

Utilisez les options principales pour definir si l'application doit :

- classer les fichiers dans des dossiers par categorie
- analyser les images
- analyser les documents
- proposer des suggestions de renommage pour les fichiers pris en charge

Si vous voulez seulement des suggestions de renommage, activez le mode correspondant.

## 3. Choisir le style de categorisation

Choisissez le style qui correspond le mieux a votre objectif :

- **More refined** pour un usage general et un classement plus detaille
- **More consistent** si vous voulez une meilleure coherence entre fichiers similaires

Vous pouvez aussi activer les listes blanches de categories pour limiter les noms de categories autorises.

## 4. Lancer l'analyse

Cliquez sur **Analyze and categorize files**.

L'application analyse le dossier selectionne, rassemble les informations necessaires et prepare une liste de verification.

## 5. Verifier avant d'appliquer

La fenetre de verification vous permet d'examiner :

- les categories proposees
- les sous-categories optionnelles
- les suggestions de renommage pour les fichiers pris en charge
- les chemins de destination finaux
- les apercus de fichiers lorsque c'est pris en charge

Vous pouvez ajuster ou refuser les suggestions avant de confirmer quoi que ce soit.

## 6. Appliquer les modifications

Une fois confirme, l'application cree les dossiers necessaires et effectue les deplacements ou renommages. Si l'approbation automatique est activee, les changements de categorie ou de nom de fichier eligibles peuvent etre appliques sans arret a la fenetre de verification.

## 7. Annuler la derniere execution

Si vous appliquez des modifications puis voulez les annuler, utilisez **Undo last run** dans le menu.

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