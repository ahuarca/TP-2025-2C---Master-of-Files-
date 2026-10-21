#!/bin/bash

# Verificación de parámetros
if [ $# -ne 2 ]; then
  echo "Uso: $0 <NRO_CARPETA> <NRO_CONFIG>"
  echo "Ejemplo: $0 2 3  --> ejecuta ./bin/master prueba2/prueba3.config"
  exit 1
fi

NRO_CARPETA="$1"
NRO_CONFIG="$2"

CARPETA="prueba${NRO_CARPETA}"
ARCHIVO="${CARPETA}/prueba${NRO_CONFIG}.config"

# Verificar carpeta
if [ ! -d "$CARPETA" ]; then
  echo "❌ No existe la carpeta: $CARPETA"
  exit 2
fi

# Verificar archivo
if [ ! -f "$ARCHIVO" ]; then
  echo "No existe el archivo de configuración: $ARCHIVO"
  exit 3
fi

echo "▶ Ejecutando master con: $ARCHIVO"
./bin/master "$ARCHIVO"
