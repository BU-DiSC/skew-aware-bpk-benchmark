import sys

runs = int(sys.argv[1])
output_dir = sys.argv[2]
input_dir = sys.argv[3]

bpk_list = ["4.0", "6.0", "8.0"]
Z_list = ["0.0", "0.01", "0.05", "0.1", "0.25", "0.5", "0.75", "1.0"]
ZD_list = ["0", "1"]
def aggregate(filename, result, print_flag=False):
    infile = open(filename, "r")
    data = infile.readlines()
    infile.close()
    for i in range(len(data)):
        tmp = data[i].strip()
        if i%(len(bpk_list) + 1) == 0:
            if len(result) <= i:
                result.append(tmp)
            if print_flag:
                print(result)
        else:
            if i >= len(result):
                result.append(float(tmp))
            else:
                result[i] += float(tmp)



def output(filename, result):
    outfile = open(filename, "w")
    i = 0
    for i in range(len(bpk_list)+1):
        j = i
        if j == 0:
            header = "bpk"
            while j < len(result):
                header += "," + result[j]
                j += len(bpk_list) + 1
            outfile.write(header+"\n")
        else:
            row = bpk_list[i-1]
            while j < len(result):
                row += "," + str(result[j]*1.0/runs)
                j += len(bpk_list) + 1
            outfile.write(row+"\n")
    outfile.close()

total_result = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
for j in range(len(ZD_list)):
    for i in range(len(Z_list)):
        for k in range(1, runs+1):
            #aggregate("output-bpk-" + str(k) + "/data_blocks" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result[j][i])
            aggregate(input_dir + str(k) + "/data_blocks" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result[j][i])
        #print(total_result[j])
        output(output_dir + "/data_blocks"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result[j][i])
        #output(output_dir + "/query_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_latency[j][i])
