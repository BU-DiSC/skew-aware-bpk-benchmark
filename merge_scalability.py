import sys

runs = int(sys.argv[1])
output_dir = sys.argv[2]

bpk_list = ["2.0", "4.0"]
Z_list = ["0.0", "1.0"]
ZD_list = ["3"]
SF_list = ["1.0", "2.0", "4.0", "8.0", "16.0"]
header = "SF,uniform,monkey,workloadaware"

def aggregate(filename, result):
    infile = open(filename, "r")
    for line in infile:
        if "point query latency:" in line:
            tmp = line.strip().split(":")
            if len(result) == 0:
                result.append(float(tmp[1].strip().split(" ")[0]))
            else:
                result[0] += float(tmp[1].strip().split(" ")[0])
        if "point query latency (monkey):" in line:
            while len(result) < 1:
                result.append(0)
            tmp = line.strip().split(":")
            if len(result) == 1:
                result.append(float(tmp[1].strip().split(" ")[0]))
            else:
                result[1] += float(tmp[1].strip().split(" ")[0])
        if "point query latency (workloadaware):" in line:
            while len(result) < 2:
                result.append(0)
            tmp = line.strip().split(":")
            if len(result) == 2:
                result.append(float(tmp[1].strip().split(" ")[0]))
            else:
                result[2] += float(tmp[1].strip().split(" ")[0])


def output(filename, result):
    outfile = open(filename, "w")
    outfile.write(header + "\n")
    for sf_index in range(len(result)):
        line = SF_list[sf_index]
        for query_lat in result[sf_index]:
            line += "," + str(query_lat)
        line += "\n"
        outfile.write(line)
    outfile.close()


total_result = [[[[[] for x in range(len(SF_list))] for l in range(len(bpk_list))] for i in range(len(Z_list))] for j in range(len(ZD_list))]

for j in range(len(ZD_list)):
    for i in range(len(Z_list)):
        for l in range(len(bpk_list)):
            for x in range(len(SF_list)):
                for k in range(1, runs+1):
                    #print("output" + str(k) + "/SF" + SF_list[x] + "/Z" + Z_list[i] + "_ZD" + ZD_list[j] + "_bpk-" + bpk_list[l])
                    aggregate("output" + str(k) + "/SF" + SF_list[x] + "/Z" + Z_list[i] + "_ZD" + ZD_list[j] + "_bpk-" + bpk_list[l] + "_output_simple_mixed_workload.txt", total_result[j][i][l][x]) 
                for y in range(len(total_result[j][i][l][x])):
                    total_result[j][i][l][x][y] /= runs

            output(output_dir + "/scalability_query_latency_Z" + Z_list[i] + "_ZD" + ZD_list[j] + "_bpk-" + bpk_list[l] + ".txt", total_result[j][i][l])

