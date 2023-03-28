#!usr/bin/env python
#-*- coding:utf-8 -*-

__author__ = 'jialongli'

import random
import copy
import pickle

class Routing:
	def __init__(self):
		self.N = 108			# number of tors
		self.MAX_HOP = 5		# number of traverse tors is +1
		self.SLICE_E = 50500	# length of active slot, unit: nanosecond
		self.SLICE_R = 10000 	# reconfiguration time, unit: nanosecond
		#self.SLICE_E = 2525	# length of active slot, unit: nanosecond
		#self.SLICE_R = 500 	# reconfiguration time, unit: nanosecond
		self.SLICE = -1 		# SLICE = SLICE_E + SLICE_R
		self.N_OCS = 6 			# 6 OCSes
		self.CONN_NUM = -1 		# how many connections during a long slice? 11
		self.PERIOD = -1 		# PERIOD = N * 2
		self.BANDWIDTH = 10 	# tor to tor bandwidth per queue, unit: Gbps
		self.DIS_TOR2TOR = 100 	# ANY tor to ANY tor distance, unit: meter
		self.PKT_SIZE = 1500 	# packet_size = 1500 bytes
		self.ONE_HOP_DELAY = -1 # ONE_HOP_DELAY = PROPA_DELAY + TRANS_DELAY
		self.PROPA_DELAY = -1 	# 5 ns for 1 m, each hop
		self.TRANS_DELAY = -1 	# transmission delay for 1500 bytes, = 1200 ns
		self.find_op_path = False	# find optimal paths with intermediate tors
		self.last_tors = [] 		# last_tors contains many lists: [tor_id, ddl(slice)]
		# intem_tors contains many lists: [layer_id, tor_id, tor_times, ddl(timing)]
		self.intem_tors = []
		# pre_tor: record each tor's predecessor and sending slice
		# key = (layer_id, tor_id, tor_times), value = (predecessor_tor_id, tor_times, sending_slice)
		self.pre_tor = {}
		# tor_present_times: record how many times a tor occurs in each layer
		# key = (layer_id, tor_id), value = tor_times
		self.tor_present_times = {}
		self.conn_timing = None
		self.ops = None
		self.all_connection = None
		self.optimal_paths = []		# contain paths with the same arriving time slot
		self.optimal_slices = []	# correspoding sending slices to these paths
		self.multihop_file = None
		self.crt_time = -1
		self.src_tor = -1
		self.dst_tor = -1
		self.one_path = []		# path with minimal hops in optimal_paths
		self.one_slice = []		# correspoding sending slices to one path
		self.one_path_len = 999
		self.file_for_all_paths = None
		self.file_for_one_path = None
		self.min_time = 9999

	# network initialization
	# a packet with 1500 bytes, 10G bandwidth link, tor to tor distance = 100 m
	# from sending to receive the whole packet, requires 500 + 1200 nanoseconds
	def init_network(self):
		self.PROPA_DELAY = self.DIS_TOR2TOR * 5 	# nanosecond
		self.TRANS_DELAY = int(self.PKT_SIZE * 8 / self.BANDWIDTH) # 1200 nanosecond
		self.ONE_HOP_DELAY = self.PROPA_DELAY + self.TRANS_DELAY
		self.SLICE = self.SLICE_E + self.SLICE_R	# nanoseconds
		self.PERIOD = self.N * 2
		# PERIOD = 216, even: length = SLICE_E; odd: length = SLICE_R
		self.CONN_NUM = self.N_OCS * 2 - 1

	# init for each src_tor to dst_tor running
	def init_each_torpair(self):
		self.optimal_paths = []
		self.optimal_slices = []
		self.one_path = []
		self.one_slice = []
		self.one_path_len = 999

	# for each tor in layer_1, should run as initial
	def init_each_last_tor(self):
		self.intem_tors = []
		self.pre_tor = {}
		self.tor_present_times = {}

	# read optical scheduling from opera
	# ops = [[[-2 for i in range(6)] for j in range(216)] for k in range(108)]
	def read_opera_scheduling(self, pkl_file):
		self.ops = pickle.load(open(pkl_file, 'rb'))

	# re_construct ops
	# all_connection = dict, key = (s_tor, d_tor), value = [connecting timings]
	def construct_ops(self):
		self.all_connection = {}
		for i in range(self.N):
			for j in range(self.N):
				if i == j:
					continue
				self.all_connection[(i, j)] = []
		for i in range(self.N):
			for j in range(self.PERIOD):
				for k in range(self.N_OCS):
					tor = self.ops[i][j][k]
					if tor != -1 and tor != i:
						self.all_connection[(i, tor)].append(j)

	# construct the conn_timing
	# conn_timing = when will s and d have connection?
	# self.conn_timing[crt_time][s][d] = [], 11 timings
	def cal_conn_timing(self):
		self.conn_timing = [[[[] for i in range(self.N)] for j in range(self.N)] for k in range(self.PERIOD)]
		for k in range(self.PERIOD):			# crt_time
			for i in range(self.N):				# s_tor
				for j in range(self.N):
					if i == j:
						continue
					for timing in self.all_connection[(i, j)]:
						if timing < k:
							timing += self.PERIOD
						self.conn_timing[k][i][j].append(timing)

	# when will s and d have connections? return a list
	def get_conn_timing(self, s, d):
		return self.conn_timing[self.crt_time][s][d]

	# when s and d have connections, return the minimum timing
	def get_min_conn_timing(self, s, d):
		return min(self.conn_timing[self.crt_time][s][d])

	# check if tor_lower_layer already occurs in tor_crt_layer's predecessor
	def is_loop(self, tor_lower_layer, tor_crt_layer, tor_times_crt_layer, layer_id):
		is_aldy_occur = False
		while (True):
			if layer_id == 1:
				break
			[tor_upper_layer, tor_times_upper_layer, sending_slice] = self.pre_tor[(layer_id, tor_crt_layer, tor_times_crt_layer)]
			if tor_lower_layer == tor_upper_layer:
				is_aldy_occur = True 
				break
			layer_id -= 1
			tor_crt_layer = tor_upper_layer
			tor_times_crt_layer = tor_times_upper_layer
		return is_aldy_occur

	# to check if a slot_idx has next slice between tor s and d
	def is_have_next_slice(self, slot_idx, s, d):
		if slot_idx in self.conn_timing[self.crt_time][s][d]:
			if (slot_idx + 1) in self.conn_timing[self.crt_time][s][d]:
				return True
		return False

	# calculate the begin and end timing of a slot
	def cal_slice_begin_end(self, slot_idx):
		# self.SLICE_E
		if slot_idx % 2 == 0:
			begin = int(slot_idx / 2) * self.SLICE
			end = begin + self.SLICE_E
		else:		# self.SLICE_R
			begin = int((slot_idx+1) / 2) * self.SLICE - self.SLICE_R
			end = begin + self.SLICE_R
		return begin, end

	# calculate the earliest and latest sending timings
	# Note that sending timing is continuous. Sending timing in one slot and
	# transmit the whole packet with part of two or more slots!
	# SLICE_E >= ONE_HOP_DELAY. SLICE_E should transfer at least one packet!
	def cal_early_late_sending(self, slot_idx, s, d):
		begin, end = self.cal_slice_begin_end(slot_idx)
		is_next_1 = self.is_have_next_slice(slot_idx, s, d)
		is_next_2 = self.is_have_next_slice(slot_idx+1, s, d)
		# SLICE_R could transmit a whole packet
		if self.SLICE_R >= self.ONE_HOP_DELAY:
			early_send = begin
			if is_next_1:
				late_send = end
			else:
				late_send = end - self.ONE_HOP_DELAY
		else:	# self.SLICE_R < self.TRANS_DELAY
			if slot_idx % 2 == 0:		# SLICE_E
				early_send = begin
				if is_next_1:
					if is_next_2:		# E R E
						late_send = end
					else:				# E R
						late_send = end + self.SLICE_R - self.ONE_HOP_DELAY
				else:					# E 
					late_send = end - self.ONE_HOP_DELAY
			else:		# SLICE_R
				if is_next_1:			# R E
					early_send = begin
					late_send = end
				else:					# R
					early_send = -1 	# SLICE_R could not serve one packet
					late_send = -1					
		return early_send, late_send

	# update parameters when adding a tor in next layer
	def add_tor_next_layer(self, layer_id, tor_next_layer, tor_crt_layer, tor_times_crt_layer, new_ddl_timing, sending_slice):
		if (layer_id+1, tor_next_layer) not in self.tor_present_times.keys():
			self.tor_present_times[(layer_id+1, tor_next_layer)] = 1
		else:
			self.tor_present_times[(layer_id+1, tor_next_layer)] += 1
		tor_times_next_layer = self.tor_present_times[(layer_id+1, tor_next_layer)]
		self.intem_tors.append([layer_id+1, tor_next_layer, tor_times_next_layer, new_ddl_timing])
		self.pre_tor[(layer_id+1, tor_next_layer, tor_times_next_layer)] = (tor_crt_layer, tor_times_crt_layer, sending_slice)

	# get the last_tors
	def get_last_tors(self):
		temp_last_tors = []
		for i in range(self.N):
			# pass the src and dst tor
			if i == self.src_tor or i == self.dst_tor:
				continue
			# minimum time from src_tor to dst_tor
			ddl = self.get_min_conn_timing(self.src_tor, self.dst_tor)
			# time from intermediate tor i to dst_tor
			for new_ddl in self.get_conn_timing(i, self.dst_tor):
				# would rather choose the direct connection from src_tor to dst_tor
				if new_ddl >= ddl:
					pass
				else:
					temp_last_tors.append([i, new_ddl])
		self.last_tors = sorted(temp_last_tors, key=lambda l:l[1], reverse=False)

	# crt_time is the sending time from the src_tor
	# get optimal path between two tors
	def get_optimal_paths(self):
		if self.MAX_HOP <= 1:
			return
		else:
			self.get_last_tors()

		self.find_op_path = False
		self.min_time = 9999
		# ith tor is the last intermediate tor, bfs-like
		for i in range(len(self.last_tors)):
			self.init_each_last_tor()
			[la_tor, ddl_dst] = self.last_tors[i]
			if ddl_dst > self.min_time:  # this last_tor is not suitable
				break
			# ddl_dst_timing: the latest time that must start sending from last_tor
			ddl_dst_timing = self.cal_early_late_sending(ddl_dst, la_tor, self.dst_tor)[1]
			# 0 = crt_layer = dst_tor's layer; 1 = tor_times_crt_layer = dst_tor's tor_times
			self.add_tor_next_layer(0, la_tor, self.dst_tor, 1, ddl_dst_timing, ddl_dst)

			# check all possible cases under a seleted la_tor
			while(True):
				# check the elements in intem_tors
				if len(self.intem_tors) == 0:
					break
				[layer_id, tor_crt_layer, tor_times_crt_layer, ddl_timing] = self.intem_tors.pop(0)
				exactly_sending = ddl_timing - self.ONE_HOP_DELAY
				if layer_id == self.MAX_HOP:
					break

				# for each tor, check the connection to src_tor
				# from src_tor to tor_crt_layer, should have enough time
				# choose the minimum timing from src_tor to tor_crt_layer
				# the earlier, the better. We just need one case
				ddl_src = self.get_min_conn_timing(self.src_tor, tor_crt_layer)
				early_sending = self.cal_early_late_sending(ddl_src, self.src_tor, tor_crt_layer)[0]
				if 0 <= early_sending <= exactly_sending:
					self.cout_op_path(tor_crt_layer, tor_times_crt_layer, layer_id, ddl_dst, ddl_src)
					self.find_op_path = True
					self.min_time = ddl_dst
					break

				# add tors from next layer, j is the next layer tor
				for j in range(self.N):
					if j == self.src_tor or j == self.dst_tor or j == tor_crt_layer:
						continue
					if self.is_loop(j, tor_crt_layer, tor_times_crt_layer, layer_id):
						continue
					for new_ddl in self.get_conn_timing(j, tor_crt_layer):
						# this tor could be an intermediate tor in next layer
						[early_sending, late_sending] = self.cal_early_late_sending(new_ddl, j, tor_crt_layer)
						# could use this tor for next layer
						if 0 <= early_sending <= exactly_sending:
							if exactly_sending < late_sending:
								new_ddl_timing = exactly_sending
							else:
								new_ddl_timing = late_sending
							self.add_tor_next_layer(layer_id, j, tor_crt_layer, tor_times_crt_layer, new_ddl_timing, new_ddl)

		# if coult not find optimal path, then direct path is the optimal
		if not self.find_op_path:
			ddl = self.get_min_conn_timing(self.src_tor, self.dst_tor)
			self.direct_path_as_optimal(ddl)
			self.one_path = [self.src_tor, self.dst_tor]
			self.one_slice = [ddl]

	# direct path is the optimal
	def direct_path_as_optimal(self, ddl):
		self.optimal_paths.append(copy.deepcopy([self.src_tor, self.dst_tor]))
		self.optimal_slices.append(copy.deepcopy([ddl]))

	# direct path info
	def get_direct_paths(self):
		time = self.get_min_conn_timing(self.src_tor, self.dst_tor)
		print('\n')
		print('direct path info')
		print([self.src_tor, self.dst_tor])
		print('the ddl_dst of this path is: ' + str(time))

	# print the path and the time
	# layer_id belongs to tor_crt_layer
	def cout_op_path(self, tor_crt_layer, tor_times_crt_layer, layer_id, ddl_dst, ddl_src):
		path = [self.src_tor, tor_crt_layer]
		sending_slice_list = [ddl_src]
		while (True):
			if layer_id == 1:
				break
			[tor_upper_layer, tor_times_upper_layer, sending_slice] = self.pre_tor[(layer_id, tor_crt_layer, tor_times_crt_layer)]
			path.append(tor_upper_layer)
			sending_slice_list.append(sending_slice)
			layer_id -= 1
			tor_crt_layer = tor_upper_layer
			tor_times_crt_layer = tor_times_upper_layer
		path.append(self.dst_tor)
		sending_slice_list.append(ddl_dst)
		#print(path)
		#print('the ddl_dst of this path is: ' + str(ddl_dst))
		self.optimal_paths.append(copy.deepcopy(path))
		self.optimal_slices.append(copy.deepcopy(sending_slice_list))
		if len(path) < self.one_path_len:
			self.one_path_len = len(path)
			self.one_path = path
			self.one_slice = sending_slice_list

	# write optimal paths to the file
	def write_optimal_paths_many_files(self):
		self.file_for_one_path.write(str(self.src_tor) + ' ' + str(self.dst_tor) + '\n')
		self.file_for_one_path.write(str(self.one_path) + '\t' + str(self.one_slice) + '\n')
		self.file_for_all_paths.write(str(self.src_tor) + ' ' + str(self.dst_tor) + '\n')
		for i in range(len(self.optimal_paths)):
			path = self.optimal_paths[i]
			sending_slice_list = self.optimal_slices[i]
			self.file_for_all_paths.write(str(path) + '\t' + str(sending_slice_list) + '\t')
		self.file_for_all_paths.write('\n')

	# write optimal paths to the file
	def write_optimal_paths(self):
		slice_idx = 0
		epoch = 0
		EPOCH = self.N * (self.N-1)
		self.multihop_file.write(str(slice_idx) + '\n')
		for path in self.optimal_paths:
			epoch += 1
			for item in path:
				self.multihop_file.write(str(item) + ' ')
			self.multihop_file.write('\n')
			if epoch == EPOCH:
				slice_idx += 1
				if slice_idx == self.N - 1:
					break
				self.multihop_file.write(str(slice_idx) + '\n')
				epoch = 0


# main function
if __name__ == '__main__':
	route = Routing()
	ops_file_path = './optical_scheduling/opera_ops'
	optimal_multihop_file_path = './optical_scheduling/multihop_' + str(route.N) + '_tors.txt'
	route.multihop_file = open(optimal_multihop_file_path, 'w')

	# running for whole simulation
	route.init_network()
	route.read_opera_scheduling(ops_file_path)
	route.construct_ops()
	route.cal_conn_timing()
	'''
	route.crt_time = 0
	route.src_tor = 0
	route.dst_tor = 1
	route.get_optimal_paths()
	ddl_dst_timing = route.cal_early_late_sending(0, 24, route.dst_tor)[1]
	print(ddl_dst_timing)
	'''
	for start_time in range(0, 216):
		route.file_for_all_paths = open('./opcc_all_optimal_paths/slice_' + str(start_time) + '.txt', 'w')
		route.file_for_one_path = open('./opcc_one_optimal_path/slice_' + str(start_time) + '.txt', 'w')
		for i in range(0, 108):
			for j in range(0, 108):
				if i == j:
					continue
				route.init_each_torpair()
				route.crt_time = start_time
				route.src_tor = i
				route.dst_tor = j
				print(start_time, i, j)
				route.get_optimal_paths()
				route.write_optimal_paths_many_files()
		route.file_for_all_paths.close()
		route.file_for_one_path.close()
	