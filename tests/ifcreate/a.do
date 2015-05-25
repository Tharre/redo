
if [ -e b ]; then
	echo -n "b" | cat - a > $3
else
	redo-ifcreate b
	if [ -e a ]; then
		echo -n "a" | cat - a > $3
	else
		echo "a" > $3
	fi
fi
