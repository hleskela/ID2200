# -*- coding: utf-8 -*-

# Written by Sam Lööf <saml@kth.se>

from math import sqrt

# Returnerar medelvärdet för lista
def mean(lista):
    return float(sum(lista)) / len(lista)

# Returnerar standardavvikelsen för lista
def standardDeviation(lista):
    summa = 0.0
    medel = mean(lista)
    for i in lista:
        summa += (i-medel)*(i-medel)
    return sqrt(float(1)/(len(lista)-1)*summa)



theFile = open("resultat.txt", "r")
lista = []
for val in theFile.read().split():
    lista.append(int(val))
theFile.close()

print 'Average: ', mean(lista)
print 'Standard deviation: ', standardDeviation(lista)
