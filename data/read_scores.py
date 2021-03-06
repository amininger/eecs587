STORING = 1
RETRIEVING = 2

def get_parens(line, num):
	open = line.find("(") + 1
	close = line.find(")") + 1
	for i in range(1, num):
		open = line.find("(", open) + 1
		close = line.find(")", close) + 1
	return line[open:close-1]

def parseFile(fin, fout, case):
	for i in range(0, 50000, 500):
		fout.write(str(i) + ", ")
	for line in fin:
		if(case == STORING and line.find("Storing") != -1):
			time = get_parens(line, 1)
			fout.write(time + ",")
		elif(case == RETRIEVING and line.find("Query") != -1): 
			time = float(line[12:len(line)-1])
			fout.write(str(time) + ",")
		elif(line.find("-- NP") != -1):
			fout.write("\n")
			fout.write(get_parens(line, 1))
			fout.write(" " + get_parens(line, 2) + ", ")
		
		

fin = open("mid_dynamic_results3", 'r')
fout = open("mid_dynamic.ret.csv", "w")
parseFile(fin, fout, RETRIEVING)

fin.close()
fout.close()
