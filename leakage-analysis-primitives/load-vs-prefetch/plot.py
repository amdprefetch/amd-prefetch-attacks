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

    labels = []

    if 'Load' in df:
        sns.lineplot(data=df, x='Index', y='Load')
        labels.append('Load')

    if 'Prefetch' in df:
        sns.lineplot(data=df, x='Index', y='Prefetch')
        labels.append('Prefetch')

    if 'NOP' in df:
        sns.lineplot(data=df, x='Index', y='NOP')
        labels.append('NOP')

    if 'PrefetchNTA' in df:
        sns.lineplot(data=df, x='Index', y='PrefetchNTA')
        labels.append('PrefetchNTA')

    plt.xlabel('Addresses')
    plt.ylabel('Time')
    plt.legend(labels=labels)

    plt.show()


if __name__ == "__main__":
    main()
