# Guia de inicio rapido

AI File Sorter le ayuda a organizar archivos con una revisión previa. La aprobación automática opcional puede preseleccionar cambios en la lista de revisión.

La IA sugiere categorías, carpetas de destino y nombres. La aplicación mueve o renombra los archivos solo después de que confirme los cambios seleccionados en el cuadro de revisión.

## Primera ejecucion segura

Si es la primera vez que usa AI File Sorter, empiece con una carpeta de prueba pequena antes de usar un archivo grande o una unidad completa.

Buenas carpetas para la primera ejecucion incluyen:

- una copia de 20-50 archivos de `Downloads`
- una carpeta pequena de capturas de pantalla o fotos para limpiar
- una carpeta temporal con algunos PDF o documentos

Esto facilita revisar la primera ejecucion. Sus archivos permanecen en su equipo cuando usa modelos locales, y la IA solo sugiere categorias y nombres. Deje desactivadas las opciones de aprobacion automatica en la primera ejecucion para que nada se mueva ni cambie de nombre hasta que apruebe la lista de revision.

## Configurar un LLM

Abra **Settings -> Select LLM**. Elija un modelo en la lista desplegable **Local LLMs** o un proveedor en **API connections**.

**Qwen3.5 4B (Recommended)** es la primera opción local y la predeterminada para configuraciones nuevas. Ofrece alta precisión de categorización y se recomienda para estructuras de carpetas existentes. Se conserva la selección de modelo guardada anteriormente. Para usar su propio archivo GGUF, elija **Custom local LLM (gguf)** y pulse **Add**.

Para un modelo local ofrecido por la aplicación, despliegue **Downloads**, pulse **Download** y espere a **Model ready** antes de confirmar con **OK**. **Model storage** permite elegir la carpeta de descarga. **See details** muestra la URL de descarga, la ruta local y la información técnica.

Para analizar imágenes, elija también un **Visual model** en **Image analysis models** y descargue todos sus archivos necesarios. Esta selección es independiente del modelo de texto.

En **API connections**, elija Gemini, ChatGPT (OpenAI) o un punto de conexión personalizado compatible con OpenAI e introduzca los datos de conexión necesarios y el nombre del modelo. Las API en la nube requieren Internet y envían los datos de análisis al servicio configurado. Los puntos de conexión personalizados también pueden ejecutarse localmente.

## Comprobar el rendimiento local (opcional)

Tras descargar los modelos, abra **File -> System compatibility check** y pulse **Run benchmark**. Cierre antes otras aplicaciones que consuman mucha CPU o GPU. La comprobación prueba los modelos locales descargados, incluidos los modelos GGUF personalizados.

Deje **Backend** en **Automatic (try CPU if GPU fails)** o elija **All available backends** para comparar la CPU y las opciones GPU compatibles. Los resultados se guardan por separado para cada modelo y motor; un fallo en uno no excluye una combinación que funcione en otro.

Seleccione un resultado y use **Retry selected** para repetir esa combinación, **Retry failed** para repetir las combinaciones fallidas o **Try chosen backend** para probar el modelo seleccionado con un motor concreto de la lista desplegable.

Las recomendaciones estiman el rendimiento local. La comprobación no prueba los servicios API ni la precisión al elegir carpetas existentes, y no cambia automáticamente el modelo seleccionado.

## 1. Elegir una carpeta

Use **Browse** o el panel **File Explorer** para elegir la carpeta que desea ordenar.

Ejemplos tipicos:

- `Downloads`
- una carpeta de limpieza del escritorio
- una carpeta de una unidad externa
- una carpeta de red o sincronizada con la nube
- un archivo de proyecto

**Destination** determina dónde se colocan los archivos ordenados. Deje **Use analyzed folder** activado para ordenar dentro de la carpeta seleccionada, o desactívelo y elija otro destino.

## 2. Elegir que debe hacer la aplicacion

Use las opciones principales para decidir si la aplicacion debe:

- categorizar archivos en carpetas
- analizar imagenes
- analizar documentos
- ofrecer sugerencias de cambio de nombre para archivos compatibles

Si solo quiere sugerencias de cambio de nombre, active el modo correspondiente de solo cambio de nombre.

## 3. Elegir cómo ordenar

Elija un modo de ordenación:

- **Create category folders** crea carpetas a partir de los nombres de categoría sugeridos.
- **Use existing folder structure** elige carpetas dentro del árbol de **Destination**. Active **Suggest new folders when needed** para permitir nuevos destinos cuando ninguno encaje bien; déjelo desactivado para usar solo carpetas existentes.

Para carpetas existentes, recomendamos **Qwen3.5 4B**. **Gemma 1.1 7B** y los modelos utilizados mediante API también son opciones adecuadas. Los demás modelos locales muestran un aviso; puede conservar su modelo o cambiarlo en **Settings -> Select LLM**.

En el modo **Create category folders**, elija un estilo de categorización:

- **More refined** para uso general y agrupacion mas detallada
- **More consistent** si desea mayor coherencia de etiquetas entre archivos similares

Tambien puede activar listas blancas de categorias si quiere que la aplicacion se mantenga dentro de un conjunto mas limitado de nombres de categorias.

## 4. Iniciar el analisis

Haga clic en **Analyze folder**.

La aplicacion escanea la carpeta seleccionada, recopila la informacion necesaria y prepara una lista de revision.

## 5. Revisar antes de aplicar cambios

El cuadro de revision le permite inspeccionar:

- categorias sugeridas
- subcategorias opcionales
- sugerencias de cambio de nombre para archivos compatibles
- las rutas finales de destino
- vistas previas de archivos cuando sean compatibles

Puede ajustar o rechazar sugerencias antes de confirmar cualquier cosa.

Active **Dry run (preview only, do not move files)** para previsualizar los movimientos y cambios de nombre previstos sin aplicarlos.

## 6. Aplicar los cambios

Seleccione las filas que desea procesar y pulse **Confirm and Process**. La aplicación crea las carpetas necesarias y realiza los movimientos o cambios de nombre seleccionados.

**Auto-approve filename changes** y **Auto-approve categorization** preseleccionan las filas aptas. Aún debe confirmar antes de que se apliquen esos cambios.

## 7. Deshacer la ultima ejecucion

Si aplica cambios y luego quiere revertirlos, use **Edit -> Undo last run** desde el menu.

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
