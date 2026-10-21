#!/bin/bash

# Verificación de argumentos
if [ $# -ne 2 ]; then
  echo "Uso: $0 <NRO_PRUEBA> <NRO_CONFIG>"
  echo "Ejemplo: $0 2 1   → ejecuta storage con prueba2/storage1.config"
  exit 1
fi

NRO_PRUEBA="$1"
NRO_CONFIG="$2"

# Archivo real en disco
REAL_PATH="config/prueba${NRO_PRUEBA}/storage${NRO_CONFIG}.config"

# Lo que se le pasa al binario
ARG_PATH="prueba${NRO_PRUEBA}/storage${NRO_CONFIG}.config"

# Verificar existencia
if [ ! -f "$REAL_PATH" ]; then
  echo "❌ No se encontró el archivo de configuración: $REAL_PATH"
  exit 2
fi

echo "▶ Ejecutando storage con: $ARG_PATH"
./bin/storage "$ARG_PATH"

