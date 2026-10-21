#!/bin/bash

if [ $# -ne 2 ]; then
  echo "Uso: $0 <CLAVE> <NUEVO_VALOR>"
  echo "Ejemplo: $0 IP_MASTER 192.168.0.10"
  exit 1
fi

CLAVE="$1"
VALOR="$2"

ERROR=0

for i in {1..6}; do
  CARPETA="prueba$i"
  
  if [ -d "$CARPETA" ]; then
    for ARCHIVO in "$CARPETA"/*.config; do
      if [ -f "$ARCHIVO" ]; then
        if grep -q "^$CLAVE=" "$ARCHIVO"; then
          sed -i "s|^$CLAVE=.*|$CLAVE=$VALOR|" "$ARCHIVO"
          echo "✅ $CLAVE actualizado en $ARCHIVO"
        else
          echo "❌ ERROR: $CLAVE no se encontró en $ARCHIVO"
          ERROR=1
        fi
      fi
    done
  else
    echo "⚠️  ADVERTENCIA: No se encontró la carpeta $CARPETA"
    ERROR=1
  fi
done

exit $ERROR
