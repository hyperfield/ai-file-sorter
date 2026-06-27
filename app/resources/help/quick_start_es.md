# Guia de inicio rapido

AI File Sorter te ayuda a organizar archivos despues de tu revision y aprobacion.

La IA dirige el analisis y sugiere categorias, subcategorias y nombres. No toca directamente tus archivos. La aplicacion realiza los movimientos o cambios de nombre solo despues de que confirmes los cambios revisados.

## 1. Elige una carpeta

Usa **Browse** para elegir la carpeta que quieres ordenar.

Ejemplos tipicos:

- `Downloads`
- una carpeta del escritorio para limpiar
- una carpeta en una unidad externa
- un archivo de proyecto

## 2. Elige lo que debe hacer la aplicacion

Usa las opciones principales para decidir si la aplicacion debe:

- clasificar archivos en carpetas por categoria
- analizar imagenes
- analizar documentos
- ofrecer sugerencias de cambio de nombre para archivos compatibles

Si solo quieres sugerencias de cambio de nombre, activa el modo correspondiente de solo renombrar.

## 3. Elige el estilo de categorizacion

Elige el estilo que mejor se adapte a tu objetivo:

- **More refined** para uso general y una agrupacion mas detallada
- **More consistent** si quieres una mayor coherencia entre archivos similares

Tambien puedes activar listas blancas de categorias si quieres que la aplicacion se limite a un conjunto mas reducido de nombres de categoria.

## 4. Inicia el analisis

Haz clic en **Analyze and categorize files**.

La aplicacion analiza la carpeta seleccionada, recopila la informacion necesaria y prepara una lista de revision.

## 5. Revisa antes de aplicar

La ventana de revision te permite comprobar:

- categorias sugeridas
- subcategorias opcionales
- sugerencias de cambio de nombre para archivos compatibles
- las rutas de destino finales

Puedes ajustar o rechazar sugerencias antes de confirmar nada.

## 6. Aplica los cambios

Una vez confirmado, la aplicacion crea las carpetas necesarias y realiza los movimientos o cambios de nombre.

## 7. Deshaz la ultima ejecucion

Si aplicas cambios y luego quieres revertirlos, usa **Undo last run** desde el menu.

La funcion de deshacer esta pensada para la ejecucion de ordenacion confirmada mas reciente. Usa el historial registrado por la aplicacion para devolver archivos a su ubicacion anterior y revertir cambios de nombre compatibles cuando sea posible.

Para obtener mejores resultados, usa deshacer antes de iniciar otra limpieza grande en la misma carpeta.

## 8. Aprendizaje a partir de tus revisiones

Cuando apruebas categorias en el dialogo de revision, la aplicacion puede recordar esas decisiones locales y usarlas como pistas en futuras ejecuciones. Esto no entrena ni modifica el modelo de IA.

Los ejemplos aprendidos se guardan en una base de datos local separada, por lo que borrar la cache normal de categorizacion no los elimina. Para borrar estos datos locales de aprendizaje, usa **Settings -> Reset learned behavior**.

## Conviene saber

- La aplicacion usa una cache local para evitar reprocesar los mismos archivos y mejorar la coherencia.
- La aplicacion no aplica cambios automaticamente sin mostrar antes el paso de revision.
- Las opciones de imagenes y documentos pueden expandirse por separado si necesitas mas control.

## Si algo parece incorrecto

Comprueba primero lo siguiente:

- la carpeta seleccionada es realmente la que querias
- las opciones de analisis relevantes estan activadas
- el modo de solo renombrar no esta limitando el resultado de forma no deseada
- una whitelist de categorias no esta restringiendo demasiado las sugerencias

Para obtener mas ayuda, abre **Help -> FAQ**.
