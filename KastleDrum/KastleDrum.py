#!/usr/bin/env python3

import argparse
import os
import struct
import subprocess
import numpy
import wavfile
from array import array


def parse_file(fobj):
    data = []
    
    for line in fobj:
        if '{' in line:
            line = line.split('{')[1].strip().strip(',')
            data.extend(int(char.strip()) for char in line.split(','))
            break
    
    for line in fobj:
        line = line.strip().strip(',')
        if '}' in line:
            line = line.split('}')[0].strip().strip(',')
            if line:
                data.extend(int(char.strip()) for char in line.split(','))
            break
        else:
            data.extend(int(char.strip()) for char in line.split(','))
    
    return data


def convert_samples(input_directory, sox, input_rate, add_empty, output_rate):
    samples = {}

    for fname in sorted(os.listdir(input_directory), key=lambda f: f.rsplit('_', 1)[0]):
        sample_name = fname.rsplit('_', 1)[0]
        if fname.endswith('_AT.h'):
            with open(f'{input_directory}/{fname}') as fobj:
                data = parse_file(fobj)
                print(f'{sample_name} => {len(data)} samples')

                raw_data = b''.join(struct.pack('<b', value) for value in data)

                subprocess.run(
                    [
                        sox, '-G', '-t', 'raw', '-r', input_rate, '-e', 'signed', '-b', '8', '-c', '1',
                        '-', f'samples/{sample_name}.wav', 'pad', '0', f'{add_empty}s', 'rate', '-v', output_rate],
                    check=True, input=raw_data)
                samples[sample_name] = open(f'samples/{sample_name}.wav', 'rb')

                #result = subprocess.run(
                #    [
                #        sox, '-G', '-t', 'raw', '-r', input_rate, '-e', 'signed', '-b', '8', '-c', '1',
                #        '-', '-t', 'raw', '-e', 'float', '-L', '-', 'rate', '-v', output_rate],
                #     check=True, input=raw_data, capture_output=True)
            
                #sample_len = int(len(result.stdout) / 4)
                #samples[sample_name] = struct.unpack('<{sample_len}f', result.stdout)
                #arr = array('f')
                #arr.frombytes(result.stdout)
                #samples[sample_name] = arr.tolist()
    return samples


def generate_wav(args):
    samples = convert_samples(args.input_directory, args.sox, args.input_rate, args.add_empty, args.output_rate)
    
    data = None
    markers = []
    rate = None
    for i, (sample_name, sample_data) in enumerate(samples.items()):
        rate, sample, bits, _1, _2 = wavfile.read(sample_data)
        print(f'Adding {sample_name} - {int(len(sample) * bits / 8)} bytes')
        markers.append(
            {'position': 0 if data is None else len(data), 'label': sample_name.encode('ascii')})
        # Concatenate samples
        if data is None:
            data = sample
        else:
            data = numpy.concatenate((data, sample))

    wavfile.write('KastleDrum.wav', rate, data, markers=markers)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Convert Bastl Kastle Drum samples to OWL resource")
    parser.add_argument('-i', '--input_directory', default='headers', help='Headers directory')
    parser.add_argument('-s', '--sox', help='Path to sox', default='/usr/bin/sox')
    parser.add_argument('-ir', '--input_rate', help='Input rate', default='16384')
    parser.add_argument('-or', '--output_rate', help='Output rate', default='16384')
    parser.add_argument('-e', '--add_empty', help='Amount of empty samples to add in the end', default='1')

    args = parser.parse_args()
    generate_wav(args)

