# Guia de inicio rapido

AI File Sorter le ayuda a organizar archivos con un flujo de trabajo de revision primero y aprobacion automatica opcional para los cambios en los que decida confiar.

La IA dirige el analisis y sugiere categorias, subcategorias y nombres. No modifica directamente sus archivos. La aplicacion mueve o cambia nombres despues de que usted confirme los cambios revisados, o automaticamente solo cuando haya activado la opcion de aprobacion automatica correspondiente.

## Primera ejecucion segura

Si es la primera vez que usa AI File Sorter, empiece con una carpeta de prueba pequena antes de usar un archivo grande o una unidad completa.

Buenas carpetas para la primera ejecucion incluyen:

- una copia de 20-50 archivos de `Downloads`
- una carpeta pequena de capturas de pantalla o fotos para limpiar
- una carpeta temporal con algunos PDF o documentos

Esto facilita revisar la primera ejecucion. Sus archivos permanecen en su equipo cuando usa modelos locales, y la IA solo sugiere categorias y nombres. Deje desactivadas las opciones de aprobacion automatica en la primera ejecucion para que nada se mueva ni cambie de nombre hasta que apruebe la lista de revision.

## 1. Elegir una carpeta

Use **Browse** o el panel **File Explorer** para elegir la carpeta que desea ordenar.

Ejemplos tipicos:

- `Downloads`
- una carpeta de limpieza del escritorio
- una carpeta de una unidad externa
- una carpeta de red o sincronizada con la nube
- un archivo de proyecto

## 2. Elegir que debe hacer la aplicacion

Use las opciones principales para decidir si la aplicacion debe:

- categorizar archivos en carpetas
- analizar imagenes
- analizar documentos
- ofrecer sugerencias de cambio de nombre para archivos compatibles

Si solo quiere sugerencias de cambio de nombre, active el modo correspondiente de solo cambio de nombre.

## 3. Seleccionar el estilo de categorizacion

Elija el estilo que mejor coincida con su objetivo:

- **More refined** para uso general y agrupacion mas detallada
- **More consistent** si desea mayor coherencia de etiquetas entre archivos similares

Tambien puede activar listas blancas de categorias si quiere que la aplicacion se mantenga dentro de un conjunto mas limitado de nombres de categorias.

## 4. Iniciar el analisis

Haga clic en **Analyze and categorize files**.

La aplicacion escanea la carpeta seleccionada, recopila la informacion necesaria y prepara una lista de revision.

## 5. Revisar antes de aplicar cambios

El cuadro de revision le permite inspeccionar:

- categorias sugeridas
- subcategorias opcionales
- sugerencias de cambio de nombre para archivos compatibles
- las rutas finales de destino
- vistas previas de archivos cuando sean compatibles

Puede ajustar o rechazar sugerencias antes de confirmar cualquier cosa.

## 6. Aplicar los cambios

Una vez que confirme, la aplicacion crea las carpetas necesarias y realiza los movimientos o cambios de nombre. Si la aprobacion automatica esta activada, los cambios de categoria o nombre de archivo elegibles pueden aplicarse sin detenerse en el cuadro de revision.

## 7. Deshacer la ultima ejecucion

Si aplica cambios y luego quiere revertirlos, use **Undo last run** desde el menu.

Deshacer esta pensado para la ejecucion de ordenacion confirmada mas reciente. Usa el historial registrado por la aplicacion para devolver archivos y revertir cambios de nombre compatibles cuando sea posible.

Para obtener mejores resultados, use Deshacer antes de iniciar otra limpieza grande en la misma carpeta.

## 8. Aprender de sus revisiones

Cuando aprueba categorias en el cuadro de revision, la aplicacion puede recordar esas decisiones locales y usarlas como pistas para futuras ejecuciones. Esto no entrena ni modifica el modelo de IA.

Los ejemplos aprendidos se almacenan en una base de datos local separada, por lo que borrar la cache normal de categorizacion no los elimina. Para eliminar estos datos de aprendizaje local, use **Settings -> Reset learned behavior**.

## Bueno saber

- La aplicacion usa una cache local para evitar reprocesar los mismos archivos y mejorar la coherencia.
- La aprobacion automatica es opcional. Dejela desactivada hasta que se sienta comodo con las sugerencias de la aplicacion.
- Las opciones de imagenes y documentos pueden expandirse por separado si necesita mas control.

## Si algo parece incorrecto

Revise primero lo siguiente:

- la carpeta seleccionada es la que queria usar
- las opciones de analisis relevantes estan activadas
- el modo de solo cambio de nombre no limita el resultado de una forma inesperada
- una lista blanca de categorias no esta restringiendo demasiado las sugerencias

Para mas solucion de problemas, abra **Help -> FAQ**.