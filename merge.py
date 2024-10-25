import sys

runs = int(sys.argv[1])
output_dir = sys.argv[2]
input_dir = "output"
if len(sys.argv) >= 4:
    input_dir = sys.argv[3]

Z_list = ["0.0", "0.5", "1.0"]
ZD_list = ["0", "1"]

def aggregate(filename, result):
    infile = open(filename, "r")
    data = infile.readlines()
    infile.close()
    for i in range(len(data)):
        tmp = data[i].strip()
        if i%(len(Z_list) + 1) == 0:
            if len(result) <= i:
                result.append(tmp)
        else:
            if i >= len(result):
                result.append(float(tmp))
            else:
                result[i] += float(tmp)



def output(filename, result):
    outfile = open(filename, "w")
    i = 0
    for i in range(len(Z_list)+1):
        j = i
        if j == 0:
            header = "Z"
            while j < len(result):
                header += "," + result[j]
                j += len(Z_list) + 1
            outfile.write(header+"\n")
        else:
            row = Z_list[i-1]
            while j < len(result):
                row += "," + str(result[j]*1.0/runs)
                j += len(Z_list) + 1
            outfile.write(row+"\n")
    outfile.close()

total_result = [[] for i in range(len(ZD_list))]
for j in range(len(ZD_list)):
    for i in range(1, runs+1):
        aggregate(input_dir + str(i) + "/data_blocks_ZD" + str(ZD_list[j]) + "_result.txt", total_result[j])
    #print(total_result[j])
    output(output_dir + "/data_blocks_ZD" + str(ZD_list[j]) + ".txt", total_result[j])
