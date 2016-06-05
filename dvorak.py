#!/usr/bin/env python

from random import choice
from editdistance import eval as editdistance
from time import time

freq_order = 'etaoinshrdlucmfwypvbgkqjxz'
dvorak_to_qwerty = dict(zip("',.pyfgcrl/=aoeuidhtns-;qjkxbmwvz",
        "qwertyuiop[]asdfghjkl;'zxcvbnm,./"))

with open('wordlist', 'r') as f:
    all_words = [line.strip() for line in f.readlines()]

def good_words(n):
    alphabet = freq_order[:n + 1]
    focus = freq_order[n]
    def good(word):
        if not all((c in alphabet for c in word)):
            return False
        if focus is not None and focus not in word:
            return False
        return True
    return [word for word in all_words if good(word)]

def genline(words):
    if words == []: return ''
    line = choice(words)
    while True:
        word = choice(words)
        if len(word) + len(line) < 80:
            line += ' ' + word
        else:
            break
    return line

def teachline(words):
    line = genline(words)
    print(line)
    start = time()
    entered = input()
    end = time()
    dtext = editdistance(line, entered)
    dtime = int(end - start)
    percentage = int(dtext / len(line))
    print(str(dtext) + ' mistakes (' + str(percentage) + '%)')
    print(str(dtime) + ' seconds')
    return dtime, dtext

def teach():
    for i in range(len(freq_order)):
        key = freq_order[i]
        print('Next key: ' + key + ' (' + dvorak_to_qwerty[key] +
            ' on qwerty keyboard)')
        words = good_words(i)
        dtime = 999999999
        dtext = 999999999
        while dtime > 100 or dtext > 1:
            dtime, dtext = teachline(words)
    while True:
        teachline(all_words)

if __name__ == '__main__':
    try:
        teach()
    except EOFError:
        pass
