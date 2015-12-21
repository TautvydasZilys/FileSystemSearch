using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace FileSystemSearch
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	public partial class SearchWindow : Window
	{
		SearchViewModel viewModel;

		public SearchWindow()
		{
			InitializeComponent();

			viewModel = new SearchViewModel();
			DataContext = viewModel;
		}

		static bool HasValidationErrors(DependencyObject obj)
		{
			return Validation.GetHasError(obj) || LogicalTreeHelper.GetChildren(obj).OfType<DependencyObject>().Any(HasValidationErrors);
		}

		private void ButtonSearch_Click(object sender, RoutedEventArgs e)
		{
			if (HasValidationErrors(this))
			{
				MessageBox.Show("One or more of search parameters are invalid.", "Search is not possible", MessageBoxButton.OK, MessageBoxImage.Error);
				return;
			}

			string validationFailedReason;
			if (!SearchUtils.ValidateSearchViewModel(viewModel, out validationFailedReason))
			{
				MessageBox.Show(validationFailedReason, "Search is not possible", MessageBoxButton.OK, MessageBoxImage.Error);
				return;
			}

			var resultWindow = new SearchResultWindow(viewModel);
			resultWindow.Show();
		}
	}
}
