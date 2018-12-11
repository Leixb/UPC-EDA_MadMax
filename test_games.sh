#!/usr/bin/env bash

GAMES=${1:-10}
PNAME=${2:-SilverBullet}
RESULT_FOLDER=${3:-tests}

mkdir -p $RESULT_FOLDER
CONT=0

for (( i = 0; i < $GAMES; i++ )); do
    SEED=$(shuf -i 0-2147483647 -n 1)
    printf -v PSEED "%010d" $SEED
    RESULT=$(./Game $PNAME Dummy Dummy Dummy -s $SEED -i default.cnf -o ${RESULT_FOLDER}/$PSEED.res 2>&1 >/dev/null | tee ${RESULT_FOLDER}/$PSEED | tail -n 2 | head -n 1 | cut -d' ' -f 3)
    echo "GAME $((i+1)) of $GAMES: SEED: $SEED. WINNER: $RESULT"
    if [ "$RESULT" = "$PNAME" ]; then
        mv "${RESULT_FOLDER}/$PSEED.res" "${RESULT_FOLDER}/W_$PSEED.res"
        mv "${RESULT_FOLDER}/$PSEED" "${RESULT_FOLDER}/W_$PSEED.txt"
        CONT=$((CONT+1))
    else
        mv "${RESULT_FOLDER}/$PSEED.res" "${RESULT_FOLDER}/L_$PSEED.res"
        mv "${RESULT_FOLDER}/$PSEED" "${RESULT_FOLDER}/L_$PSEED.txt"
    fi
done

echo "Won $CONT of $GAMES"
