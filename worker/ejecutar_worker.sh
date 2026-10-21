#!/bin/bash

# Verificación de argumentos
if [ $# -ne 3 ]; then
  echo "Uso: $0 <NRO_PRUEBA> <NRO_CONFIG> <ID_WORKER>"
  echo "Ejemplo: $0 2 1 5   → ejecuta worker con prueba2/worker1.config y worker_id=5"
  exit 1
fi

NRO_PRUEBA="$1"
NRO_CONFIG="$2"
ID_WORKER="$3"

# Ruta del archivo de configuración
CONFIG_PATH="prueba${NRO_PRUEBA}/worker${NRO_CONFIG}.config"

# Verificar que exista el archivo
if [ ! -f "$CONFIG_PATH" ]; then
  echo "❌ No se encontró el archivo de configuración: $CONFIG_PATH"
  exit 2
fi

# Ejecutar worker
echo "▶ Ejecutando worker ID=$ID_WORKER con config: $CONFIG_PATH"
./bin/worker "$CONFIG_PATH" "$ID_WORKER"
