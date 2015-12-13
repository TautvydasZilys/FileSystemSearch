using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;

namespace FileSystemSearch
{
	/// <summary>
	/// Interaction logic for SearchResultWindow.xaml
	/// </summary>
	public partial class SearchResultWindow : Window
	{
		private ObservableCollection<SearchResultViewModel> resultList;

        private SearchUtils.FoundPathCallbackDelegate foundPathCallback;
        private SearchUtils.SearchProgressUpdatedDelegate searchProgressUpdatedCallback;
        private SearchUtils.SearchDoneCallbackDelegate searchDoneCallback;

		public SearchResultWindow(SearchViewModel searchViewModel)
		{
			InitializeComponent();

            headerStackPanel.DataContext = searchViewModel;

			resultList = new ObservableCollection<SearchResultViewModel>();
			resultListView.ItemsSource = resultList;

            progressBar.IsEnabled = true;
            progressBar.Value = 0;
            progressBar.IsIndeterminate = true;

            foundPathCallback = OnPathFound;
            searchProgressUpdatedCallback = OnProgressUpdated;
            searchDoneCallback = OnSearchDone;

			SearchUtils.SearchAsync(searchViewModel, foundPathCallback, searchProgressUpdatedCallback, searchDoneCallback);
		}

		private void ListViewItem_MouseDoubleClick(object sender, MouseButtonEventArgs e)
		{
			var item = (sender as ListView).SelectedItem as SearchResultViewModel;

            try
            {
                System.Diagnostics.Process.Start(item.Path);
            }
            catch (Exception ex)
            {
                MessageBox.Show(string.Format("Failed to open {0}: {1}.", item.Path, ex.Message), "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
		}

		private void OnPathFound(string path)
		{
			Dispatcher.InvokeAsync(() =>
			{
				resultList.Add(new SearchResultViewModel(path));
			}, DispatcherPriority.Background);
		}

        private void OnProgressUpdated(double progress)
        {
            Dispatcher.InvokeAsync(() =>
            {
                progressBar.IsIndeterminate = false;
                progressBar.Value = progress;
            }, DispatcherPriority.Background);
        }

        private void OnSearchDone()
        {
            Dispatcher.InvokeAsync(() =>
            {
                progressBar.IsIndeterminate = false;
                progressBar.IsEnabled = false;
                progressBar.Value = 100;
            }, DispatcherPriority.Background);
        }
	}
}
