#!/usr/bin/env bash

GAMES=${1:-10}
PNAME=${2:-Demo}
RESULT_FOLDER=${3:-tests}

mkdir -p $RESULT_FOLDER
CONT=0

POINTS=0

echo "-------------------------------------------------------------------------------"
echo Player: $PNAME
echo Folder: $RESULT_FOLDER
echo Games:  $GAMES
echo "-------------------------------------------------------------------------------"

for (( i = 1; i <= $GAMES; i++ )); do
    SEED=$(shuf -i 0-2147483647 -n 1)
    printf -v SEED "%010d" $SEED
    OUT_FILE="${RESULT_FOLDER}/${SEED}"

    echo -ne "Game $i/$GAMES\tSEED: $SEED RUNNING ...\r"

    ./Game $PNAME Dummy Dummy Dummy -s $SEED -i default.cnf -o "${OUT_FILE}.res" 2>"${OUT_FILE}"

    WINNER=$(tail -n 2 "${OUT_FILE}" | head -n 1 | cut -d' ' -f 3)

    POINTS_PNAME=$(tail -n 6 ${OUT_FILE}| grep "${PNAME} got score" | cut -d' ' -f 6 | sort | head -n 1)
    POINTS_WINNER=$(tail -n 6 ${OUT_FILE}| grep "${WINNER} got score" | cut -d' ' -f 6 | sort | head -n 1)
    POINTS=$((POINTS+POINTS_PNAME))

    echo -e "Game $i/$GAMES\tSEED: $SEED WINNER: $WINNER ($POINTS_WINNER)"

    if [ "$WINNER" = "$PNAME" ]; then
        mv "${OUT_FILE}.res" "${RESULT_FOLDER}/W_$SEED.res"
        mv "${OUT_FILE}" "${RESULT_FOLDER}/W_$SEED.txt"
        CONT=$((CONT+1))
    else
        mv "${OUT_FILE}.res" "${RESULT_FOLDER}/L_$SEED.res"
        mv "${OUT_FILE}" "${RESULT_FOLDER}/L_$SEED.txt"
        echo -e "\t\t\t\t\t $PNAME ($POINTS_PNAME)"
    fi
done

echo "-------------------------------------------------------------------------------"

echo "WON: $CONT/$GAMES ($(((CONT*100)/GAMES))%)"
echo "MEAN: $((POINTS/GAMES))"

echo "-------------------------------------------------------------------------------"

