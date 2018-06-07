for p in $(pwd)/sherpya/*; do
  echo "- Appling $(basename $p)"
  patch -d ../.. -p1 < $p || return 1
done
