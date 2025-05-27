echo "// File generated automatically by make\nconst char TcpSlaveIdentifier[40] = \"ID: git commit time $(git log -1 --format='%cd' --date=format:'%Y-%m-%d %H:%M:%S')\";" > git_revision.cpp

