import sys

runs = int(sys.argv[1])
output_dir = sys.argv[2]

bpk_list = ["2.0", "5.0"]
Z_list = ["0.0" , "1.0"]
ZD_list = ["0", "3"]
header = ""

def aggregate(filename, result, run_idx):
    infile = open(filename, "r")
    data = infile.readlines()
    infile.close()
    for i in range(len(data)):
        if i >= len(result):
            result.append(data[i].strip())
        else:
            tmp = data[i].strip().split(',')
            if tmp[0] != "ops":
                tmp2 = result[i].split(',')
                agg_tmp = [0.0 for _ in tmp2]
                for j in range(len(tmp2)):
                    agg_tmp[j] = (float(tmp2[j])*run_idx + float(tmp[j]))*1.0/(run_idx + 1)
                result[i] = ','.join([str(x) for x in agg_tmp])


def output(filename, result):
    outfile = open(filename, "w")
    outfile.write(header)
    for line in result:
        outfile.write(line + "\n")
    outfile.close()



total_result = [[[[] for l in range(len(bpk_list))] for i in range(len(Z_list))] for j in range(len(ZD_list))]
for j in range(len(ZD_list)):
    for i in range(len(Z_list)):
        for l in range(len(bpk_list)):
            result = []
            for k in range(1, runs+1):
                aggregate("output" + str(k) + "/Z" + Z_list[i] + "_ZD" + str(ZD_list[j]) + "-bpk-" + bpk_list[l] + "_throughputs_simple_exp.txt", result, k-1)
            output(output_dir + "/throughput_Z" + Z_list[i] + "_ZD" + str(ZD_list[j]) + "-bpk-" + bpk_list[l] + "_simple_exp.txt", result)
