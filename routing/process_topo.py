#!usr/bin/env python
#-*- coding:utf-8 -*-

__author__ = 'jialongli'
# written on 20220125

import random
import copy
import pickle

class Topo:
	def __init__(self):
		self.topo_file_path = None
		self.pkl = None
		self.ops = [[[-2 for i in range(6)] for j in range(216)] for k in range(108)]

	# construct ops.pkl file
	def process(self):
		file = open(self.topo_file_path, 'r')
		period = 0
		line_count = 0
		for line in file.readlines():
			if line_count == 326:
				break
			if line_count == 0 or line_count == 1:
				line_count += 1
				continue

			if line_count % 3 == 2:
				line = line.split(' ')
				for i in range(648):
					m = i % 6
					n = int(i / 6)
					self.ops[n][period][m] = int(line[i])
				period += 1
				line_count += 1
				continue
			if line_count % 3 == 0:
				line_count += 1
				continue

			if line_count % 3 == 1:
				line = line.split(' ')
				for i in range(648):
					m = i % 6
					n = int(i / 6)
					self.ops[n][period][m] = int(line[i])
				period += 1
				line_count += 1
				continue
		pickle.dump(self.ops, open(self.pkl, 'wb'))

	# translate the optimal path in opera to all tors
	def translate_queue_to_tor(self, o_path, t_slice, file):
		file = open(self.topo_file_path, 'r')
		line_count = 0
		src_tor = o_path[0]
		dst_tor = o_path[1]
		path = [src_tor]
		queue = o_path[2:]
		crt_tor = src_tor
		for line in file.readlines():
			if line_count == 326:
				break
			if line_count == 0 or line_count == 1:
				line_count += 1
				continue
			if line_count == t_slice + 2:
				line = line.split(' ')
				for i in range(len(queue)):
					q = queue[i]
					next_tor = int(line[crt_tor*6 + q-6])
					crt_tor = next_tor
					path.append(next_tor)
				print(path)
				break
			line_count += 1
		return path

	@staticmethod
	def check_opera_paths_same_3slices():
		queue_file = open('./optical_scheduling/dynexp_N=108_k=12_1path_only_queue.txt', 'r')
		slot_count = -1
		for line in queue_file.readlines():
			line = line.split(' ')
			if len(line) == 1:
				slot = int(line[0])
				if slot % 3 == 0:
					slot_count += 1
					path_file = open('./opera_paths/000slice_' + str(slot_count*2) + '.txt', 'w')
					path_file.write('slice index: ' + str(slot_count*2) + '\n')
				continue
			if slot % 3 == 1 or slot % 3 == 2:
				continue

			p = list(map(int, line))
			translate_path = topo.translate_queue_to_tor(p, slot, file)
			path_file.write(str(translate_path[0]) + ' ' + str(translate_path[-1]) + '\n')
			path_file.write(str(translate_path) + '\n')

	@staticmethod
	def cal_hops():
		folder = './opcc_paths/E_10100_R_2000/opcc_one_optimal_path/slice_'
		max_hop = 5
		hop_count = [0 for i in range(max_hop)]
		for i in range(216):
			file = open(folder + str(i) + '.txt', 'r')
			print(i)
			lines = file.readlines()
			for j in range(1, 23112):
				if j % 2 == 0:
					continue
				line = lines[j].split(' ')
				#print(line)
				hop = len(line) - 1
				#print(len(line))
				hop_count[hop-1] += 1
		print(hop_count)

	@staticmethod
	def cal_hops_with_slices():
		folder = './opcc_paths/E_8585_R_1700_with_slices/opcc_one_optimal_path/slice_'
		max_hop = 5
		hop_count = [0 for i in range(max_hop)]
		for i in range(216):
			file = open(folder + str(i) + '.txt', 'r')
			print(i)
			lines = file.readlines()
			for j in range(1, 23112):
				if j % 2 == 0:
					continue
				line = lines[j].strip('\n').split('\t')[0]
				line = line.split(' ')
				#print(line)
				hop = len(line) - 1
				#print(len(line))
				hop_count[hop-1] += 1
		print(hop_count)

	@staticmethod
	def check_opera_path_found():
		opera_folder = './opera_paths/slice_'
		opcc_folder = './opcc_all_optimal_paths/slice_'
		for i in range(108):
			opera_file = open(opera_folder + str(i*2) + '.txt', 'r')
			opcc_file = open(opcc_folder + str(i*2) + '.txt', 'r')
			opera_lines = opera_file.readlines()
			opcc_lines = opcc_file.readlines()
			for j in range(2, 23113):
				opera_line = opera_lines[j].strip('\n')
				opcc_line = opcc_lines[j-1].strip('\n').split('\t')
				if opera_line not in opcc_line:
					if len(opcc_line) != 2 and len(opcc_line[0].split(',')) != 2:
						print(i, j)
						print('False')
						print(opera_line)
						print(opcc_line)
				#break
			break

	@staticmethod
	def check_opcc_paths():
		folder = './opcc_paths/E_5050_R_1000/opcc_all_optimal_paths/slice_'
		for i in range(216):
			opcc_file = open(folder + str(i) + '.txt', 'r')
			lines = opcc_file.readlines()
			if lines[0][0] != '0' or lines[0][2] != '1':
				print(i)
				print('error1')
			if lines[23110][0:7] != '107 106':
				print(i)
				print(lines[23110][0:7])
				print('error2')
			

	# translate tor to queue and write file
	@staticmethod
	def translate_tor_to_queue():
		tor_folder = './opcc_paths/E_2525_R_500/opcc_one_optimal_path/slice_'
		tor_file = open('./optical_scheduling/dynexp_N=108_k=12_1path_only_tors.txt', 'r')
		opcc_file = open('./opcc_paths/opcc_paths_E_2525_R_500.txt', 'w')
		tor_lines = tor_file.readlines()
		for line in tor_lines:
			opcc_file.write(line)
		for i in range(216):
			print(i)
			#slice_lines = file.readlines()
			for j in range(3):		# write three times
				opcc_file.write(str(i*3+j) + '\n')
				line_count = 0
				file = open(tor_folder + str(i*2) + '.txt')
				for line in file.readlines():
					line = line.strip('\n')
					line = line.strip('[]')
					if line_count % 2 == 0:
						opcc_file.write(line)
						line_count += 1
						continue
					else:
						path = line.split(', ')
						index_line = tor_lines[i*3+2].strip('\n').split(' ')
						for hop in range(len(path) - 1):
							s = int(path[hop])
							d = path[hop+1]
							q = int(index_line[6*s: 6*s+6].index(d)) + 6
							opcc_file.write(' ' + str(q))
						line_count +=1
					opcc_file.write('\n')
				file.close()	

	# check if the opcc path file is good after tuning to queues
	@staticmethod
	def check_opcc_paths_in_queue():
		opcc_file = open('./opcc_paths/opcc_paths_E_50500_R_10000.txt', 'r')
		for i, line in enumerate(opcc_file):
			if i < 327:
				continue
			line = line.strip('\n').split(' ')
			if len(line) == 2 or len(line) > 7:
				print(line)
				print(i)

	@staticmethod
	def translate_tor_only_first_tor():
		tor_folder = './opcc_paths/E_5050_R_1000/opcc_one_optimal_path/slice_'
		tor_file = open('./optical_scheduling/dynexp_N=108_k=12_1path_only_tors.txt', 'r')
		opcc_file = open('./opcc_paths/opcc_paths_E_5050_R_1000.txt', 'w')
		tor_lines = tor_file.readlines()
		for i, line in enumerate(tor_lines):
			if i == 1:
				opcc_file.write('216 5050000 1000000' + '\n')
				continue
			if i % 3 == 0 and i > 0:
				continue
			opcc_file.write(line)
		for i in range(216):
			print(i)
			#slice_lines = file.readlines()
			opcc_file.write(str(i) + '\n')
			line_count = 0
			file = open(tor_folder + str(i) + '.txt')
			for line in file.readlines():
				line = line.strip('\n')
				line = line.strip('[]')
				if line_count % 2 == 0:
					#opcc_file.write(line)
					line_count += 1
					continue
				else:
					path = line.split(', ')
					src_tor = path[0]
					dst_tor = path[-1]
					first_hop_tor = path[1]
					opcc_file.write(src_tor + ' ' + dst_tor + ' ' + first_hop_tor)
					line_count +=1
				opcc_file.write('\n')
			file.close()	

	@staticmethod
	def translate_tor_only_first_tor_with_slices():
		SLICE_E = 2525
		SLICE_R = 500
		NUM_SLICE = 216
		tor_folder = './opcc_paths/E_' + str(SLICE_E) + '_R_' + str(SLICE_R)+ '/opcc_one_optimal_path/slice_'
		tor_file = open('./optical_scheduling/dynexp_N=108_k=12_1path_only_tors.txt', 'r')
		opcc_file = open('./opcc_paths/opcc_paths_E_' + str(SLICE_E) + '_R_' + str(SLICE_R)+'.txt', 'w')
		tor_lines = tor_file.readlines()
		for i, line in enumerate(tor_lines):
			if i == 1:
				opcc_file.write(str(NUM_SLICE) + ' ' +str(SLICE_E*1000)+' '+ str(SLICE_R*1000) + '\n')
				continue
			if i % 3 == 0 and i > 0:
				continue
			opcc_file.write(line)
		for i in range(NUM_SLICE):
			print(i)
			opcc_file.write(str(i) + '\n')
			line_count = 0
			file = open(tor_folder + str(i) + '.txt')
			for line in file.readlines():
				if line_count % 2 == 0:
					line_count += 1
					continue
				else:
					line = line.strip('\n').split('\t')[0]
					line = line.strip('[]')
					path = line.split(', ')
					src_tor = path[0]
					dst_tor = path[-1]
					first_hop_tor = path[1]
					opcc_file.write(src_tor + ' ' + dst_tor + ' ' + first_hop_tor)
					line_count +=1
				opcc_file.write('\n')
				#break
			file.close()	

	@staticmethod
	def translate_tor_only_first_queue_with_slices():
		SLICE_E = 1894
		SLICE_R = 375
		NUM_SLICE = 216
		tor_folder = './opcc_paths/E_' + str(SLICE_E) + '_R_' + str(SLICE_R)+ '_with_slices/opcc_one_optimal_path/slice_'
		tor_file = open('./optical_scheduling/dynexp_N=108_k=12_1path_only_tors.txt', 'r')
		opcc_file = open('./opcc_paths/opcc_paths_E_' + str(SLICE_E) + '_R_' + str(SLICE_R)+'.txt', 'w')
		tor_lines = tor_file.readlines()
		for i, line in enumerate(tor_lines):
			if i == 1:
				opcc_file.write(str(NUM_SLICE) + ' ' +str(SLICE_E*1000)+' '+ str(SLICE_R*1000) + '\n')
				continue
			if i % 3 == 0 and i > 0:
				continue
			opcc_file.write(line)
		for i in range(NUM_SLICE):
			print(i)
			opcc_file.write(str(i) + '\n')
			line_count = 0
			file = open(tor_folder + str(i) + '.txt')
			for line in file.readlines():
				if line_count % 2 == 0:
					line_count += 1
					continue
				else:
					[line, sending_slices] = line.strip('\n').split('\t')
					line = line.strip('[]')
					path = line.split(', ')
					sending_slices = sending_slices.strip('[]')
					first_hop_slice = sending_slices.split(', ')[0]
					src_tor = path[0]
					dst_tor = path[-1]
					first_hop_tor = path[1]
					opcc_file.write(src_tor + ' ' + dst_tor)
					
					if int(first_hop_slice) > NUM_SLICE - 1:
						slice_idx = int(first_hop_slice) % NUM_SLICE
						first_hop_slice = str(int(first_hop_slice) % NUM_SLICE)
					else:
						slice_idx = int(first_hop_slice)

					if slice_idx % 2 == 0:
						idx = int(slice_idx/2*3) + 2
					else:
						idx = int((slice_idx-1)/2*3) + 2 + 2
					#print(i, first_hop_slice, idx)
					index_line = tor_lines[idx].strip('\n').split(' ')
					#print(first_hop_tor)
					#print(index_line[6*int(src_tor): 6*int(src_tor)+6])
					q = int(index_line[6*int(src_tor): 6*int(src_tor)+6].index(first_hop_tor)) + 6
					opcc_file.write(' ' + str(q) + ' ' + first_hop_slice)

					line_count +=1
				if i == NUM_SLICE - 1 and src_tor == '107' and dst_tor == '106':
					continue
				else:
					opcc_file.write('\n')
				#break
			file.close()	


if __name__ == '__main__':
	topo = Topo()
	topo.topo_file_path = './optical_scheduling/dynexp_N=108_k=12_1path_only_tors.txt'
	topo.translate_tor_only_first_queue_with_slices()
	#topo.cal_hops_with_slices()
