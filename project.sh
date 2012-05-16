if [ -z "$1" ]
then
	mf="gedit"
else
	if [[ "$1" == "--help" || "$1" == "help" || "$1" == "-help" || "$1" == "-h" ]]
	then
		echo -e "Opens the source files for you in a friendly order.\nDefaults to gedit; use project.sh \"<COMMAND>\" to specify your own."
		exit;
	fi
	mf="$1"
fi
$mf quickdrawer.c quickdrawer.h stored.c stored.h preferences.c preferences.h highlight.c highlight.h Makefile
