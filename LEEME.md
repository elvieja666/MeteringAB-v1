# Metering A/B

Plugin de matcheo de loudness Send/Receive para Logic. Ponés un Metering A/B antes de tu cadena de efectos y otro después: el segundo iguala automáticamente el volumen, así comparás el sonido con y sin efectos sin que te engañe el "más fuerte suena mejor". Todas las perillas aceptan valores exactos escritos con el teclado.

---

Hay dos formas de instalarlo. Elegí una:

- **Opción A · Que GitHub lo compile.** No instalás nada de desarrollo: GitHub arma el plugin en una Mac en la nube y vos bajás el archivo terminado.
- **Opción B · Compilarlo en tu Mac.** Instalás dos herramientas y lo compilás vos.

---

## Opción A · GitHub lo compila por vos

### A1. Subir el proyecto a GitHub (una sola vez)

1. Creá una cuenta gratis en <https://github.com> (si no tenés).
2. Arriba a la derecha tocá **+** → **New repository**.
   - *Repository name:* `MeteringAB`
   - Puede ser **Private** o **Public**, las dos funcionan.
   - **No** marques ninguna casilla de "Add a README" ni similares.
   - Tocá **Create repository**.
3. En la página que aparece, tocá el link **uploading an existing file**.
4. En el Finder, abrí la carpeta `MeteringAB` y apretá **Cmd + Shift + .** (punto). Así aparecen los archivos ocultos, incluida la carpeta **.github**, que es la que le dice a GitHub cómo compilar.
5. Seleccioná **todo** lo que hay adentro de la carpeta (Cmd + A), incluida `.github`, y arrastralo a la página de GitHub.
6. Esperá a que suban los archivos y tocá el botón verde **Commit changes**.

### A2. Esperar la compilación

1. Arriba en tu repositorio tocá la pestaña **Actions**.
2. Vas a ver "Compilar Metering A/B" con un círculo amarillo girando: está compilando. Tarda unos 10 a 15 minutos.
3. Cuando aparece un tilde verde ✅, está listo. Si aparece una cruz roja ❌, tocala, sacá una captura y mandámela.

### A3. Bajar e instalar el plugin

1. Volvé a la página principal del repositorio. A la derecha, en **Releases**, tocá **Metering A/B (última versión)**.
2. Bajá **MeteringAB-mac.zip**. Si Safari no lo descomprime solo, hacé doble clic: queda una carpeta **MeteringAB-mac** en Descargas.
3. Abrí la **Terminal** (Cmd + Espacio, escribí "Terminal"), pegá esto y apretá Enter:

```
bash ~/Downloads/MeteringAB-mac/instalar.command
```

Cuando dice **✅ Listo**, pasá a **Usarlo en Logic**, más abajo.

> Cada vez que cambies algo del código en GitHub, se compila solo de nuevo y aparece la versión nueva en Releases.

---

## Opción B · Compilarlo en tu Mac

### B1. Herramientas de desarrollo de Apple

Abrí la app **Terminal** (Cmd + Espacio, escribí "Terminal") y pegá esto:

```
xcode-select --install
```

Aparece una ventana: tocá **Instalar** y aceptá. Tarda unos minutos. No hace falta el Xcode completo de la App Store.

### B2. CMake

Es el programa que arma el proyecto.

1. Entrá a <https://cmake.org/download/> y bajá el instalador para **macOS** (archivo `.dmg`).
2. Abrilo y arrastrá **CMake** a la carpeta Aplicaciones.

(Si usás Homebrew, alcanza con `brew install cmake`.)

### B3. Compilar e instalar

1. Descomprimí el zip en tu carpeta **Documentos**, para que quede `Documentos/MeteringAB`.
2. En la Terminal escribí:

```
cd ~/Documents/MeteringAB
./compilar.sh
```

La primera vez descarga JUCE y compila: puede tardar entre 5 y 15 minutos. Cuando termina dice **✅ Listo** y el plugin queda instalado solo.

> Si macOS dice "permiso denegado", escribí primero `chmod +x compilar.sh` y volvé a probar.

---

## Usarlo en Logic

1. Abrí Logic. Si te pregunta por plugins nuevos, dejá que los valide.
   Si no aparece: menú **Logic Pro > Configuración (Settings) > Plug-in Manager**, buscá Metering A/B y tocá **Reset & Rescan Selection**.
2. En la pista donde querés comparar:
   - **Primer slot:** Metering A/B → modo **Send (antes de FX)**, Link ID **1**.
   - **En el medio:** tus efectos (compresor, EQ, saturador, lo que sea).
   - **Último slot:** otro Metering A/B → modo **Receive (después de FX)**, Link ID **1**.
3. Dale play. En el Receive tiene que decir en verde **Conectado al Send**.

### Los controles (en el Receive)

| Control | Qué hace |
|---|---|
| **Procesado / Original / Delta** | El A/B. *Procesado* = con tus efectos, al mismo volumen que el original. *Original* = la señal antes de los efectos. *Delta* = solo lo que tus efectos le cambiaron al sonido. |
| **Matcheo** | *Auto* iguala solo. *Manual* usa la perilla Ganancia manual. *Apagado* no corrige nada. |
| **Medición** | *LUFS* se parece más a cómo escucha el oído (recomendado). *RMS* es energía pura. |
| **Ventana** | Cuánto tiempo promedia la medición. Más larga = más estable; más corta = reacciona más rápido. |
| **Límite auto** | Máxima corrección que puede aplicar el modo Auto, para arriba o para abajo. |
| **Congelar** | Deja fija la corrección actual del modo Auto. |
| **Latencia** | Compensa efectos que retrasan el audio (EQ de fase lineal, limitadores con lookahead). |

### Valores exactos

- **Clic en el número** debajo de cada perilla, escribí el valor (por ejemplo `-3.25`) y Enter. También acepta coma: `-3,25`.
- **Shift + arrastrar** = ajuste fino.
- **Doble clic** en la perilla = vuelve al valor por defecto.

### Tips

- **Cada pista con su propio Link ID** (1 a 16). Si dos Send usan el mismo número, Metering A/B te avisa en rojo.
- **Cómo encontrar la latencia correcta:** poné el Receive en *Delta* y subí la Latencia de a poco. El valor correcto es donde la señal Delta suena más baja o más "limpia". Con la mayoría de compresores y EQ comunes es 0.
- El Send no cambia el audio para nada; solo lo "mira".

---

## Para curiosos: cómo está armado

- `Source/PluginProcessor.cpp` · el procesamiento de audio (Send y Receive).
- `Source/LoudnessMeter.h` · el medidor RMS / LUFS (filtro K de la norma ITU-R BS.1770).
- `Source/SharedBus.h` · el "cable invisible" que conecta el Send con el Receive.
- `Source/PluginEditor.cpp` · la interfaz (perillas, botones, colores).
- `Tests/` · pruebas automáticas que verifican que el matcheo funcione.
- `.github/workflows/compilar.yml` · las instrucciones para que GitHub compile el plugin.
- `instalar.command` · instala el plugin que compiló GitHub.

Si cambiás algo del código, volvé a correr `./compilar.sh` (Opción B) o subí el cambio a GitHub (Opción A), y reabrí el plugin en Logic.
