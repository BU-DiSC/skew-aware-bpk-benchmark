import argparse, os

bpk_list = ["2.0","3.0","4.0","5.0","6.0","7.0"]

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



def output(filename, result, runs):
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


def main():
    parser = argparse.ArgumentParser(description="Aggregate results from multiple experimental runs.")

    parser.add_argument("runs", type=int, help="The total number of runs to aggregate.")
    parser.add_argument("output_dir", type=str, help="Directory to save the final aggregated files.")
    parser.add_argument("input_dir", type=str, help="Base directory containing the individual run folders.")

    parser.add_argument("--Z-list", type=float, nargs='+', default=[0.0,0.5,1.0], help="A space-separated list of Z values to process. Default: [0.0,0.5,1.0]")
    parser.add_argument("--ZD-list", type=int, nargs='+', default=[0,1], help="A space-separated list of ZD values to process. Default: [0,1]")

    args = parser.parse_args()

    metrics = {
        "read_bytes": [[[] for _ in args.Z_list] for _ in args.ZD_list],
        "query_latency": [[[] for _ in args.Z_list] for _ in args.ZD_list],
        "accessed_data_blocks": [[[] for _ in args.Z_list] for _ in args.ZD_list]
    }

    # --- Main Processing Loop ---
    for j, zd_val in enumerate(args.ZD_list):
        for i, z_val in enumerate(args.Z_list):
            for k in range(1, args.runs + 1):
                run_dir = args.input_dir + str(k)
                
                for metric_name, results_matrix in metrics.items():
                    filename = f"{metric_name}_ZD{zd_val}_Z{z_val}_result.txt"
                    filepath = os.path.join(run_dir, filename)
                    aggregate(filepath, results_matrix[j][i])

            # --- Output Final Aggregated Files ---
            for metric_name, results_matrix in metrics.items():
                output_filename = f"{metric_name}_Z{z_val}_ZD{zd_val}.txt"
                output_filepath = os.path.join(args.output_dir, output_filename)
                output(output_filepath, results_matrix[j][i], args.runs)
    
    print("Aggregation complete.")
    print(f"Results saved in: {args.output_dir}")

if __name__ == "__main__":
    main()

'''
total_result_read_bytes = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_point_query_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_ingestion_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_avg_total_latency = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
total_result_accessed_data_blocks = [[[] for j in range(len(Z_list))] for i in range(len(ZD_list))]
for j in range(len(ZD_list)):
    for i in range(len(Z_list)):
        for k in range(1, runs+1):
        #for k in range(6, runs+6):
            aggregate(input_dir + str(k) + "/read_bytes" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i]) + "_result.txt", total_result_read_bytes[j][i])
            aggregate(input_dir + str(k) + "/query_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])  + "_result.txt", total_result_point_query_latency[j][i])
            #aggregate(input_dir + str(k) + "/write_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_ingestion_latency[j][i])
            #aggregate(input_dir + str(k) + "/avg_latency" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i])+ "_result.txt", total_result_avg_total_latency[j][i])
            aggregate(input_dir + str(k) + "/accessed_data_blocks" + "_ZD" + str(ZD_list[j])  + "_Z" + str(Z_list[i]) + "_result.txt", total_result_accessed_data_blocks[j][i])
        #print(total_result[j])
        output(output_dir + "/read_bytes"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_read_bytes[j][i])
        output(output_dir + "/query_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_point_query_latency[j][i])
        #output(output_dir + "/write_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_ingestion_latency[j][i])
        #output(output_dir + "/avg_latency"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_avg_total_latency[j][i])
        output(output_dir + "/accessed_data_blocks"+ "_Z" + str(Z_list[i]) + "_ZD" + str(ZD_list[j]) + ".txt", total_result_accessed_data_blocks[j][i])
'''
