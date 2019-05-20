#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse

import numpy as np
import matplotlib.pyplot as plt
import scipy.fftpack

import pqf

SAMPLES = pqf.frame_sz
RUNS = 3
SAMPLERATE = 44100

def show_proto():
    proto = pqf.prototype()
    fig = plt.figure()

    yf = scipy.fftpack.fft(proto[:len(proto)])

    fig.add_subplot(111).plot(10 * np.log10(np.abs(yf[:len(proto)//2])))
    fig.add_subplot(222).plot(proto)
    plt.show()


def do_an(ctx, time, data, shift):
    data_slice = data[shift : SAMPLES + shift]
    time_slice = time[shift : SAMPLES + shift]

    res = ctx.do(data_slice);

    fig = plt.figure()

    a = fig.add_subplot(111)
    a.plot(time_slice, res)
    a.set_title("output")
    b = fig.add_subplot(222)
    b.plot(time_slice, data_slice)
    b.set_title("input");
    plt.show()


def process_freq(freq):
    ctx = pqf.AnalyzeCtx()

    time = np.arange(0, SAMPLES * RUNS, 1)
    amplitude = 0.01 * np.sin(2 * np.pi * freq * time / SAMPLERATE).astype(np.float32)

    do_an(ctx, time, amplitude, 0);
    do_an(ctx, time, amplitude, SAMPLES);


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--freq', type=int, help="generate sine with given frequency", action='store')
    parser.add_argument('--proto', help="show filter prototype", action='store_true')

    args = parser.parse_args()

    if (args.proto):
        show_proto()
    else:
        process_freq(args.freq)


if __name__ == "__main__":
    main()
