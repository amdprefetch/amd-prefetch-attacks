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
    raw_mode = True if "_raw.csv" in path1 else False

    labels = [x for x in df if x != 'Cycle']

    if raw_mode:
        df = df.mask(df.sub(df.mean()).div(df.std()).abs().gt(2))
        # sns.histplot(df, binwidth=1, legend=True, multiple='stack')
        # sns.displot(df, binwidth=1, legend=True)

        for v in df:
            sns.kdeplot(df[v], label=v, legend=True)
    else:
        ax = None
        for v in labels:
            ax = sns.lineplot(x=df['Cycle'], y=df[v], ax=ax, label=v)

    plt.legend()
    plt.xlabel('Prefetch Time')
    plt.ylabel('# Cases')
    plt.grid(False)

    plt.show()

if __name__ == "__main__":
    main()
