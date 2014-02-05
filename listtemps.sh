list() {
egrep "getI\(\"\.|getInt\(\"\." *.cc | cut -d '"' -f 2
egrep "getST\(|getIT\(|getITd\(|setST\(|setIT\(" *.cc | cut -d '"' -f 2 | cut -d '(' -f 2 | cut -d ')' -f 1
}

list | sort | uniq
