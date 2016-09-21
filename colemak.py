#!/usr/bin/env python3

# TODO: Record word accuracy, including typos, repeated typos using backspace,
# total time to write word, all divided by word length. Then store words that
# are hard and let them recur more often
# TODO: Add punctuation on more advanced levels
# TODO: Factor in timing into letter accuracy and global accuracy

# DONE: Statistics, line generation

from editdistance import eval as editdist
from time import time # as now

import random
import curses
import sys
import os

os.environ.setdefault('ESCDELAY', '0')

freq_order = 'etaoinshrdlucmfwypvbgkqjxz'

MAX_DELAY = 1.0 # If it takes longer to press a key, this is used instead

key_acc = [0.1] * 26
key_acc_mod = [0.9] * 26
accuracy = 0.1
accuracy_mod = 0.9
unlocked = 2
last_time = time()
avg_time = MAX_DELAY

def get_time():
    global last_time, avg_time
    now = time()
    diff = now - last_time
    last_time = now
    if diff > MAX_DELAY:
        diff = MAX_DELAY
    if diff > 0.01:
        avg_time = 0.1 * diff + 0.9 * avg_time
    return diff

def update_acc(pressed, target, time, word):
    global key_acc, key_acc_mod, accuracy, accuracy_mod
    assert isinstance(pressed, str) and len(pressed) == 1, repr(pressed)
    assert 'a' <= pressed <= 'z' or pressed in ' ', repr(pressed)
    assert isinstance(target, str) and len(target) <= 1, repr(target)
    assert 'a' <= target <= 'z' or target in ' ', repr(target)
    assert isinstance(time, float)
    hit = int(pressed == target)
    # global accuracy
    mod = accuracy_mod
    accuracy_mod = 0.05 + 0.9 * accuracy_mod
    accuracy = mod * hit + (1 - mod) * accuracy

    # target accuracy
    if target not in ' ':
        i = ord(target) - ord('a')
        mod = key_acc_mod[i]
        key_acc_mod[i] = 0.1 + 0.8 * key_acc_mod[i]
        key_acc[i] = mod * hit + (1 - mod) * key_acc[i]
    # word accuracy TODO
    # pressed accuracy TODO
    # TODO: Handle time
    return 1 - hit

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
    assert len(weighted) > 0
    total = sum(w for w, _ in weighted)
    def get_word():
        x = random.uniform(0, total)
        upto = 0
        for w, word in weighted:
            upto += w
            if upto > x:
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
    return line

def redraw_rest(current, pos, stdscr, force=False):
    if pos < len(current) or force:
        y, x = stdscr.getyx()
        stdscr.move(y, pos)
        stdscr.clrtoeol()
        stdscr.addstr(current[pos:])

def do_line(words, stdscr):
    global unlocked
    target = ' '.join(words)
    stdscr.addstr(target + '\n')
    escapeseq = ''
    current = ''
    ch = ''
    pos = 0
    get_time()
    total_time = 0
    fails = 0
    while ch not in ['\n']:
        ch = stdscr.getkey()
        delay = get_time()
        total_time += delay
        wi = current[:pos].count(' ')
        y, x = stdscr.getyx()
        if len(ch) == 1 and ('a' <= ch <= 'z' or ch in ' '):
            if ch != target[pos:pos + 1]:
                fails += 1
            current = current[:pos] + ch + current[pos:]
            update_acc(ch, target[pos:pos + 1], delay, words[wi])
            pos += 1
            stdscr.addstr(ch)
            redraw_rest(current, pos, stdscr)
        elif ch == 'KEY_LEFT' and pos > 0:
            pos -= 1
        elif ch == 'KEY_RIGHT' and pos < len(current):
            pos += 1
        elif ch == 'KEY_BACKSPACE' and pos > 0:
            pos -= 1
            current = current[:pos] + current[pos + 1:]
            redraw_rest(current, pos, stdscr, True)
        elif ch == 'KEY_DC' and pos < len(current):
            current = current[:pos] + current[pos + 1:]
            redraw_rest(current, pos, stdscr, True)
        elif ch == 'KEY_END':
            pos = len(current)
        elif ch == 'KEY_HOME':
            pos = 0
        elif ch == '\x1b':
            escapeseq = ch
        elif ch == '\n':
            stdscr.addstr('\n')
            break
#        elif ch == 'KEY_RESIZE':
#            stdscr.redrawwin()
#        else:
#            stdscr.addstr(repr(ch))
        stdscr.move(y, pos)
    wpm = len(target) * 12 / total_time
    acc = (len(target) - fails) / len(target)
    edd = editdist(current, target)
    stdscr.addstr('WPM: {:.2f}  Accuracy: {:.2%}\n'.format(wpm, acc))
    if wpm > 32 and acc > 0.98:
        unlocked += 1
    return (wpm, acc, edd)

def main(stdscr):
    while True:
        words = genline()
        results = do_line(words, stdscr)

try:
    curses.wrapper(main)
except KeyboardInterrupt:
    print('Goodbye!')
