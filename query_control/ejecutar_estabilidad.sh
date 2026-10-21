#!/bin/bash

# Número de instancias
INSTANCIAS=25

# Config común
CONFIG="queryControl.config"

# Nombre de queries y prioridades fijas
declare -a NOMBRES=("AGING_1" "AGING_2" "AGING_3" "AGING_4")
declare -a PRIORIDADES=(4 3 5 1)

echo "▶ Ejecutando $INSTANCIAS instancias de query_control con AGING_1..4"

for ((i=1; i<=INSTANCIAS; i++)); do
    echo "---------------------------------------------"
    echo "▶ INICIANDO INSTANCIA $i"
    echo "---------------------------------------------"

    for idx in {0..3}; do
        NOMBRE="${NOMBRES[$idx]}"
        PRIORIDAD="${PRIORIDADES[$idx]}"

        echo "Lanzando ./bin/query_control $CONFIG $NOMBRE $PRIORIDAD"

        ./bin/query_control "$CONFIG" "$NOMBRE" "$PRIORIDAD" &
    done

    echo "✔ Instancia $i lanzada"
    sleep 0.1
done

echo "---------------------------------------------"
echo "✔ Todas las instancias fueron lanzadas."
echo "---------------------------------------------"
