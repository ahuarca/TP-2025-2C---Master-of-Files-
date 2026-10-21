#!/bin/bash

# Debe recibir exactamente 2 parámetros
if [ $# -ne 2 ]; then
  echo "Uso: $0 <CLAVE> <NUEVO_VALOR>"
  echo "Ejemplo: $0 IP_MASTER 127.0.0.1"
  exit 1
fi

CLAVE="$1"
VALOR="$2"
CONFIG_FILE="queryControl.config"

# Verificar que existe el archivo
if [ ! -f "$CONFIG_FILE" ]; then
  echo "❌ No existe el archivo: $CONFIG_FILE"
  exit 2
fi

# Verificar que la clave exista
if ! grep -q "^$CLAVE=" "$CONFIG_FILE"; then
  echo "❌ La clave '$CLAVE' no existe en $CONFIG_FILE"
  exit 3
fi

# Actualizar valor
sed -i "s|^$CLAVE=.*|$CLAVE=$VALOR|" "$CONFIG_FILE"

echo "✔ $CLAVE actualizado correctamente en $CONFIG_FILE"
