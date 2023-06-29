# Converts 3-slice Opera to 2-slice Opera by merging first 2 slices
N_slices = 324
N_superslices = 324 // 3

with open('dynexp_N=108_k=12_1path.txt', 'r') as readf:
    with open('operavlb_from_dynexp_N=108_k=12_1path.txt', 'w') as writef:
        writef.write(readf.readline())
        writef.write(readf.readline())
        for i in range(N_superslices):
            writef.write(readf.readline())
            readf.readline()
            writef.write(readf.readline())
            

        counter = 0
        for i in range(N_superslices):
            readf.readline()
            writef.write(str(counter) + '\n')
            for j in range(107 * 108):
                writef.write(readf.readline())
            readf.readline()
            for j in range(107 * 108):
                readf.readline()

            counter += 1

            readf.readline()
            writef.write(str(counter) + '\n')
            for j in range(107 * 108):
                writef.write(readf.readline())
            
            counter += 1

            
