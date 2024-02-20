import sys

runs = int(sys.argv[1])
output_dir = sys.argv[2]

bpk_list = ["2.0","3.0","4.0","5.0","6.0","7.0"]
Z_list = ["0.0", "0.5", "1.0"]
ZD_list = ["0", "3"]
#bpk_list = ["1.0", "9.0", "12.0"]
#Z_list= ["0.0"]
#ZD_list= ["3"]
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

total_result_read_bytes = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_point_query_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_ingestion_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_avg_total_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_accessed_data_blocks = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
for j in range(len(ZD_list)):
    for i in range(len(Z_list)):
        for k in range(1, runs+1):
        #for k in range(6, runs+6):
            aggregate("output" + str(k) + "/read_bytes" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_read_bytes[j][i])
            aggregate("output" + str(k) + "/query_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_point_query_latency[j][i])
            aggregate("output" + str(k) + "/write_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_ingestion_latency[j][i])
            aggregate("output" + str(k) + "/avg_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_avg_total_latency[j][i])
            aggregate("output" + str(k) + "/accessed_data_blocks" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_accessed_data_blocks[j][i])
        #print(total_result[j])
        output(output_dir + "/read_bytes"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_read_bytes[j][i])
        output(output_dir + "/query_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_point_query_latency[j][i])
        output(output_dir + "/write_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_ingestion_latency[j][i])
        output(output_dir + "/avg_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_avg_total_latency[j][i])
        output(output_dir + "/accessed_data_blocks"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_accessed_data_blocks[j][i])
