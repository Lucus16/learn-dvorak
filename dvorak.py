#!/usr/bin/env python

from random import choice, random
from editdistance import eval as editdistance
from time import time

freq_order = 'etaoinshrdlucmfwypvbgkqjxz'

with open('wordlist', 'r') as f:
    all_words = [line.strip() for line in f.readlines()]

def good_words(n):
    if n < len(freq_order):
        letters = set(freq_order[:n + 1])
        focus = freq_order[n]
    else:
        return all_words, all_words
    lwords = [word for word in all_words if set(word) <= letters]
    fwords = [word for word in lwords if focus in word]
    return fwords, lwords

def genline(fwords, lwords):
    line = choice(fwords)
    while True:
        if random() < 0.8:
            word = choice(fwords)
        else:
            word = choice(lwords)
        if len(word) + len(line) < 80:
            line += ' ' + word
        else:
            return line

def teachline(fwords, lwords):
    line = genline(fwords, lwords)
    print(line)
    start = time()
    entered = input()
    end = time()
    dtext = editdistance(line, entered)
    dtime = int(end - start)
    percentage = int(dtext / len(line) * 100)
    print('{} mistakes ({}%), {} seconds'.format(dtext, percentage, dtime))
    return dtime, dtext

def teach():
    i = 1
    words = good_words(i)
    progress = 0
    while True:
        key = freq_order[i]
        print('Current key:', key, '\t\tcurrent progress:', progress)
        input('Press enter when ready')
        dtime, dtext = teachline(*words)
        if dtime > 60:
            progress -= 1
            if progress < 0:
                progress = 1
                i -= 1
                words = good_words(i)
        if dtime > 30 or dtext > 1:
            progress = 0
        else:
            progress += 1
        if progress >= 2:
            progress = 0
            i += 1
            words = good_words(i)

if __name__ == '__main__':
    try:
        teach()
    except EOFError:
        pass
    print()
