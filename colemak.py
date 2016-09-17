#!/usr/bin/env python3

# TODO: Record word accuracy, including typos, repeated typos using backspace,
# total time to write word, all divided by word length. Then store words that
# are hard and let them recur more often
# TODO: Add punctuation on more advanced levels

# DONE: Statistics, line generation

from editdistance import eval as editdist
from time import time

import random
import curses
import os

os.environ.setdefault('ESCDELAY', '0')

freq_order = 'etaoinshrdlucmfwypvbgkqjxz'

key_acc = [0.0] * 26
key_acc_mod = [1.0] * 26
accuracy = 0.0
accuracy_mod = 1.0
unlocked = 1
last_time = time()
avg_time = 2.0

def get_time():
    now = time()
    diff = now - last_time
    last_time = now
    if diff > 2.0:
        diff = 2.0
    avg_time = 0.1 * diff + 0.9 * avg_time
    return diff

def update_acc(key, hit, time):
    assert isinstance(key, str) and len(key) == 1
    assert 'a' <= key <= 'z'
    assert isinstance(hit, bool)
    i = ord(key) - ord('a')
    mod = key_acc_mod[i]
    key_acc_mod[i] = 0.1 + 0.8 * key_acc_mod[i]
    key_acc[i] = mod * int(hit) + (1 - mod) * key_acc[i]
    mod = accuracy_mod
    accuracy_mod = 0.05 + 0.9 * accuracy_mod
    accuracy = mod * int(hit) + (1 - mod) * accuracy

def get_acc(key):
    assert isinstance(key, str) and len(key) == 1
    assert 'a' <= key <= 'z'
    i = ord(key) - ord('a')
    return key_acc[i]

def avg(l):
    return sum(l) / len(l)

with open('wordlist', 'r') as f:
    wordlist = [line.strip() for line in f.readlines()]

def rate_word(word):
    for c in word:
        if c in freq_order[unlocked:]:
            return 0.0
    p = 1.0
    for c in freq_order[:unlocked]:
        if c not in word:
            p *= get_acc(c) / accuracy
    return p * avg([1 - get_acc(c) for c in word])

def genline():
    weighted = ((rate_word(w), w) for w in wordlist)
    weighted = [(w, x) for w, x in weighted if w > 0.0]
    total = sum(w for w, _ in weighted)
    def get_word():
        x = random.uniform(0, total)
        upto = 0
        for w, word in weighted:
            upto += w
            if upto > r:
                return word
        return weighted[-1]
    line = []
    size = -1
    while size < 72:
        word = get_word()
        line.append(word)
        size += len(word) + 1
    if size > 80:
        line = line[:-1]
    return ' '.join(line)


def main(stdscr):
    ch = ''
    while ch != 'q':
        ch = stdscr.getkey()
        stdscr.addstr(ch)

curses.wrapper(main)
