from sympy import primerange

primes = list(primerange(2, 300))[:52]

primorials = []
current = 1
for p in primes:
    current *= p
    primorials.append(str(current))

print("const char* primorials[52] = {")
for val in primorials:
    print(f'    "{val}",')
print("};")
