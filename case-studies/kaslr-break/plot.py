#!/usr/bin/env python

import pandas as pd
import os
import click
import numpy as np
import math
import seaborn as sns
import matplotlib.pyplot as plt

@click.command()
@click.argument('path1', type=click.Path(exists=True))
@click.argument('output', type=click.Path(), required=False)
def main(path1, output):
    df = pd.read_csv(path1)

    plt.plot(df['Time'])

    plt.xlabel('Offset')
    plt.ylabel('Prefetch Time')
    plt.grid(False)

    plt.show()

if __name__ == "__main__":
    main()
