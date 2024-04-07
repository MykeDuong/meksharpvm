import subprocess
import os


# Path to the directory containing the sample files
sample_dir = './samples'

# Path to the program executable
program_path = './mkv.out'

# Ensure the sample directory exists
if not os.path.exists(sample_dir):
    print(f"The directory {sample_dir} does not exist.")
    exit(1)

# Iterate through all files in the sample directory
for filename in os.listdir(sample_dir):
    file_path = os.path.join(sample_dir, filename)
    
    # Check if the current item is a file
    if os.path.isfile(file_path):
        print(f"---------- {filename} ----------")
        
        # Execute the program with the current file as the argument
        result = subprocess.run([program_path, file_path], capture_output=True, text=True)
        
        # Print the output from the program
        print(result.stdout)
        print(result.stderr)
