#!/bin/bash
# ---------------------------------------------------------------------------
#  Compila Metering A/B e instala el plugin (AU para Logic + VST3).
#  Uso: abrí la Terminal en esta carpeta y escribí:  ./compilar.sh
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")"

# 1) Buscar CMake (instalado con Homebrew o como app en /Applications).
if command -v cmake >/dev/null 2>&1; then
    CMAKE=cmake
elif [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then
    CMAKE="/Applications/CMake.app/Contents/bin/cmake"
else
    echo "❌ No encontré CMake. Instalalo (paso 2 del LEEME) y volvé a probar."
    exit 1
fi

# 2) Verificar las herramientas de desarrollo de Apple.
if ! xcode-select -p >/dev/null 2>&1; then
    echo "❌ Faltan las herramientas de desarrollo de Apple."
    echo "   Escribí:  xcode-select --install   y volvé a probar cuando termine."
    exit 1
fi

echo "🔧 Configurando (la primera vez descarga JUCE, puede tardar unos minutos)..."
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release

echo "🔨 Compilando..."
"$CMAKE" --build build --config Release --target MeteringAB_AU MeteringAB_VST3 -j "$(sysctl -n hw.ncpu)"

# 3) Avisarle a macOS que hay un Audio Unit nuevo.
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "🔍 Validando el Audio Unit (lo mismo que hace Logic)..."
if auval -v aufx Mtab Ms06 2>&1 | grep -q "AU VALIDATION SUCCEEDED"; then
    echo "✅ Validación OK."
else
    echo "⚠️  La validación no dio OK todavía. Probá abrir Logic igual; si no aparece,"
    echo "   corré:  auval -v aufx Mtab Ms06   y pasame lo que dice."
fi

echo ""
echo "✅ Listo. Metering A/B quedó instalado en:"
echo "   ~/Library/Audio/Plug-Ins/Components/Metering AB.component  (Logic)"
echo "   ~/Library/Audio/Plug-Ins/VST3/Metering AB.vst3"
echo "   Abrí Logic y buscalo en Audio Units > 0600musica > Metering A/B."
