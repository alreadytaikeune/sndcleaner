import csv
import matplotlib.pyplot as plt
import pandas as pd
from math import exp, pi, sqrt, pow
from sklearn.decomposition import PCA
from sklearn import svm
from sklearn.cross_validation import train_test_split

df = pd.read_csv("train.csv", sep=" ", header=None)
print len(df)


def gen_cumulated_gaussian(n, stdd, m, l):
	out=[]
	inc=float(l)/n
	x=0
	out.append(0)
	for k in range(1,n):
		out.append(out[k-1]+2*(1./(sqrt(2*pi)*stdd))*exp(-pow(x-m, 2)/(2*pow(stdd, 2)))*inc)
		x+=inc
	return out


def cum_gauss(gauss, x, n, l):
	n=int(x/l)
	s=float(x/l)
	return (1-s+n)*gauss[n]+(s-n)*gauss[n+1]
speech_nb=0.
speech_correct=0.
music_nb=0.
music_correct=0.

label=df[21].as_matrix()
df=df.drop(df.columns[[21]], axis=1)
df=df.apply(lambda x : (x - x.mean())/(x.max()-x.min()))
pca = PCA(copy=True, n_components=0.7) # 70% of variance to explain

X_train, X_test, y_train, y_test = train_test_split(df, label, test_size=0.2)
 
 
pca.fit(X_train)
print(pca.n_components_) 
print(pca.explained_variance_ratio_)
tX_train=pca.transform(X_train)
tX_test=pca.transform(X_test)


clf = svm.SVC()
clf.fit(tX_train, y_train)

pred=clf.predict(tX_test)
print pred
print y_test
plt.figure(0)
print len(y_test)
print len(pred)
for k in range(len(pred)):
	if pred[k]=="music":
		music_nb+=1
		if pred[k]==y_test[k]:
			music_correct+=1
	else:
		speech_nb+=1
		if pred[k]==y_test[k]:
			speech_correct+=1

print "speech correct: {0}".format(speech_correct/speech_nb)
print "music correct: {0}".format(music_correct/music_nb)


for k in range(len(tX_test)):
	if(y_test[k]=="music"):
		col="red"
	else:
		col="blue"
	plt.scatter(tX_test[k][0], tX_test[k][1], color=col)
plt.figure(1)


for k in range(len(pred)):
	
	if(pred[k]=="music"):
		col="red"
	else:
		col="blue"
	plt.scatter(tX_test[k][0], tX_test[k][1], color=col)

	
plt.show()














