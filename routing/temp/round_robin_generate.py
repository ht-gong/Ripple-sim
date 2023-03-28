tope = 12

# modificar el numero de clubes
clubes = []
index_clubes = 0
for i in range(0,tope):
   clubes.append(str(i))

auxT = len(clubes)
impar= True if auxT%2 != 0 else False

if impar:
   auxT += 1

totalP = int(auxT/2) # total de partidos de una jornada
jornada = []
indiceInverso = auxT-2


for i in range(1,auxT):
   equipos = []
   list_equipos = {}
   for indiceP in range(0,totalP):
      if index_clubes > auxT-2:
         index_clubes = 0

      if indiceInverso < 0:
         indiceInverso = auxT-2

      if indiceP == 0: # seria el partido inicial de cada fecha
         if impar:
            equipos.append(clubes[index_clubes])
         else:
            if (i+1)%2 == 0:
               partido = [clubes[index_clubes], clubes[auxT-1]]
            else:
               partido = [clubes[auxT-1], clubes[index_clubes]]
            equipos.append(" ".join(partido))
      else:
         partido = [clubes[index_clubes], clubes[indiceInverso]]
         equipos.append(" ".join(partido))
         indiceInverso -= 1
      index_clubes += 1

   list_equipos = {
      'jornada': "Jornada Nro.: " + str(i),
      'equipos': equipos
   }
   jornada.append(list_equipos)


#print(jornada)
ops = [[-1 for i in range(tope-1)] for j in range(tope)]
for item in jornada:
   for key, value in item.items():
      if key == 'jornada':
         round_id = int(value.split(':')[-1])
      else:
         for teams in value:
            team_1 = int(teams.split(' ')[0])
            team_2 = int(teams.split(' ')[1])
            ops[team_1][round_id-1] = team_2
            ops[team_2][round_id-1] = team_1
   
for i in range(tope):
   #print(ops[i])
   pass

for i in range(tope):
   if i in ops[i]:
      print('false')

col_totals = [ sum(x) for x in zip(*ops)]
for item in col_totals:
   if item != int((tope-1)*tope/2):
      print('false')

f = open('./optical_scheduling/round_robin_' + str(tope) +'_tors.txt', 'w')
for i in range(tope):
   for item in ops[i]:
      f.write(str(item) + ' ')
   f.write('\n')



'''
for item in jornada:
   for key, value in item.items():
      print(value)
   break
a = 'Jornada Nro.: 1'
b = ['0 19', '1 18', '2 17', '3 16', '4 15', '5 14', '6 13', '7 12', '8 11', '9 10']
line = a.split(':')
print(int(line[-1]))
for item in b:
   line = item.split(' ')
   print(line)
'''