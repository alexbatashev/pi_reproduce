import sys
import json

def main():
    if len(sys.argv) != 3:
        print("Usage: python graph2dot.py path/to/trace/graph.json out.dot")

    with open(sys.argv[1], 'r') as f:
        graph = f.read()

    graphObj = json.loads(graph)

    with open(sys.argv[2], 'w') as f:
        f.write('digraph {\n')
        for obj in graphObj:
            if obj["kind"] == "node":
                f.write("n{} [label = \"Name: {}\\nDevice: {}\\n".format(obj["id"],
                    obj["name"], obj["sycl_device"]))
                if "allocation_type" in obj:
                    f.write("Allocation type: {}\\n".format(obj["allocation_type"]))
                if "copy_from" in obj:
                    f.write("{} -> {}\\n".format(obj["copy_from"], obj["copy_to"]))
                if "kernel_name" in obj:
                    f.write("Kernel: {}\\n".format(obj["kernel_name"]))
                if "sym_line_no" in obj:
                    f.write("Source file: {}\\nSource line: {}".format(obj["sym_source_file_name"], obj["sym_line_no"]))
                f.write("\"];\n")
            else:
                if "access_mode" in obj:
                    label = obj["access_mode"]
                else:
                    label = "dealloc"
                f.write("n{} -> n{} [label=\"{}\"];\n".format(obj["source"],
                    obj["target"], label))
        f.write('}')

if __name__ == "__main__":
    main()
