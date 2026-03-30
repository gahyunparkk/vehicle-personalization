#!/bin/bash
# tools/sync_shared.sh

SRC="Shared"
TARGETS=(
    "Ecu_Safety_State_TC375/Shared"
    "Ecu_Access_Body_TC275/Shared"
    "Ecu_Hvac_Hmi_TC375/Shared"
)

for target in "${TARGETS[@]}"; do
    cp -r $SRC/* $target/
    echo "Synced → $target"
done